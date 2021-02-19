#ifdef ENABLE_OPENCL

//******************************************************************************
// system includes
//******************************************************************************

#include <monitor.h>                          // monitor_real_abort
#include <math.h>															// pow
#include <stdbool.h>                          // bool
#include <ucontext.h>                         // getcontext



//******************************************************************************
// local includes
//******************************************************************************

#include "blame-kernel-map.h"                 // kernel_node_t
#include "blame-queue-map.h"                  // queue_node_t
#include "blame-helper.h"              				// calculate_blame_for_active_kernels
#include "blame.h"

#include <hpcrun/cct/cct.h>                   // cct_node_t
#include <hpcrun/memory/hpcrun-malloc.h>      // hpcrun_malloc_safe
#include <hpcrun/safe-sampling.h>             // hpcrun_safe_enter, hpcrun_safe_exit
#include <hpcrun/sample_event.h>              // hpcrun_sample_callpath

#include <lib/prof-lean/spinlock.h>           // spinlock_t, SPINLOCK_UNLOCKED
#include <lib/prof-lean/stdatomic.h>          // atomic_fetch_add
#include <lib/support-lean/timer.h>           // time_getTimeReal



//******************************************************************************
// local data
//******************************************************************************

int cpu_idle_metric_id;
int gpu_time_metric_id;
int cpu_idle_cause_metric_id;
int gpu_idle_metric_id;

// lock for blame-shifting activities
static spinlock_t itimer_blame_lock = SPINLOCK_UNLOCKED;

static _Atomic(uint64_t) g_num_threads_at_sync = { 0 };
static _Atomic(uint32_t) g_unfinished_kernels = { 0 };

_Atomic(unsigned long) latest_kernel_end_time = { 0 };

static _Atomic(kernel_node_t*) completed_kernel_list_head = { NULL };

static queue_node_t *queue_node_free_list = NULL;
static kernel_node_t *kernel_node_free_list = NULL;



//******************************************************************************
// private operations
//******************************************************************************

static queue_node_t*
queue_node_alloc_helper
(
 queue_node_t **free_list
)
{
  queue_node_t *first = *free_list;

  if (first) {
    *free_list = atomic_load(&first->next);
  } else {
    first = (queue_node_t *) hpcrun_malloc_safe(sizeof(queue_node_t));
  }

  memset(first, 0, sizeof(queue_node_t));
  return first;
}


static void
queue_node_free_helper
(
 queue_node_t **free_list,
 queue_node_t *node
)
{
  atomic_store(&node->next, *free_list);
  *free_list = node;
}


static kernel_node_t*
kernel_node_alloc_helper
(
 kernel_node_t **free_list
)
{
  kernel_node_t *first = *free_list;

  if (first) {
    *free_list = atomic_load(&first->next);
  } else {
    first = (kernel_node_t *) hpcrun_malloc_safe(sizeof(kernel_node_t));
  }
  memset(first, 0, sizeof(kernel_node_t));
  return first;
}


static void
kernel_node_free_helper
(
 kernel_node_t **free_list,
 kernel_node_t *node
)
{
  atomic_store(&node->next, *free_list);
  *free_list = node;
}


static void
add_queue_node_to_splay_tree
(
 uint64_t queue_id
)
{
  queue_node_t *node = queue_node_alloc_helper(&queue_node_free_list);
  atomic_init(&node->next, NULL);
  queue_map_insert(queue_id, node);
}


// Insert a new node in kernel_map corresponding to a kernel
static void
create_and_insert_kernel_entry
(
 uint64_t kernelexec_id,
 cct_node_t *launcher_cct
)
{
  kernel_node_t *kernel_node = kernel_node_alloc_helper(&kernel_node_free_list);
  kernel_node->kernel_id = kernelexec_id;
  kernel_node->launcher_cct = launcher_cct;
  atomic_init(&kernel_node->next, NULL);
  kernel_map_insert(kernelexec_id, kernel_node);
}


static void
record_latest_kernel_end_time
(
 unsigned long kernel_end_time
)
{
  unsigned long expected = atomic_load(&latest_kernel_end_time);
  if (kernel_end_time > expected) {
    atomic_compare_exchange_strong(&latest_kernel_end_time, &expected, kernel_end_time);
  }
}


static void
add_kernel_to_completed_list
(
 kernel_node_t *kernel_node
)
{
  while (true) {
    kernel_node_t *current_head = atomic_load(&completed_kernel_list_head);
    atomic_store(&kernel_node->next, current_head);
    if (atomic_compare_exchange_strong(&completed_kernel_list_head, &current_head, kernel_node)) {
      break;
    }
  }
}


static void
attributing_cpu_idle_metric_at_sync_epilogue
(
 cct_node_t *cpu_cct_node,
 double sync_start,
 double sync_end
)
{
  // attributing cpu_idle time for the min of (sync_end - sync_start, sync_end - last_kernel_end)
  uint64_t last_kernel_end_time = atomic_load(&latest_kernel_end_time);

  // this differnce calculation may be incorrect. We might need to factor the different clocks of CPU and GPU
  // using time in seconds may lead to loss of precision
  uint64_t cpu_idle_time = ((sync_end - last_kernel_end_time) < (sync_end - sync_start)) ?
    (sync_end - last_kernel_end_time) :	(sync_end  - sync_start);
  cct_metric_data_increment(cpu_idle_metric_id, cpu_cct_node, (cct_metric_data_t) {.i = (cpu_idle_time)});
}


// this function returns a list of kernel nodes that have been completely processed
// uses may be do free resources related to the processed kernels e.g in opencl, we can call clReleaseEvent
static kernel_id_t
attributing_cpu_idle_cause_metric_at_sync_epilogue
(
 double sync_start,
 double sync_end
)
{
	kernel_id_t processed_ids = {0, NULL};
  kernel_node_t *private_completed_kernel_head = atomic_load(&completed_kernel_list_head);
  if (private_completed_kernel_head == NULL) {
    // no kernels to attribute idle blame, return
    return processed_ids;
  }
  kernel_node_t *null_ptr = NULL;
  if (atomic_compare_exchange_strong(&completed_kernel_list_head, &private_completed_kernel_head, null_ptr)) {
    // exchange successful, proceed with metric attribution

    calculate_blame_for_active_kernels(private_completed_kernel_head, sync_start, sync_end);
    kernel_node_t *curr = private_completed_kernel_head;
    kernel_node_t *next;
		long length = 0;
    while (curr) {
      next = atomic_load(&curr->next);
			length++;
      curr = next;
    }

    uint64_t *id = hpcrun_malloc_safe(sizeof(uint64_t) * length);  // how to free reuse array data?
    long i = 0;
    curr = private_completed_kernel_head;

    while (curr) {
      cct_metric_data_increment(cpu_idle_cause_metric_id, curr->launcher_cct,
          (cct_metric_data_t) {.r = curr->cpu_idle_blame});
      next = atomic_load(&curr->next);
			id[i++] = curr->kernel_id;
      kernel_map_delete(curr->kernel_id);
      kernel_node_free_helper(&kernel_node_free_list, curr);
      curr = next;
    }
		processed_ids.id = id;
		processed_ids.length = length;
  } else {
    // some other sync block could have attributed its idleless blame, return
  }
	return processed_ids;
}


static uint32_t
get_count_of_unfinished_kernels
(
 void
)
{
  return atomic_load(&g_unfinished_kernels);
}



//******************************************************************************
// interface operations
//******************************************************************************

void
queue_prologue
(
 uint64_t queue_id
)
{
  // prevent self a sample interrupt while gathering calling context
  hpcrun_safe_enter();

  add_queue_node_to_splay_tree(queue_id);

  hpcrun_safe_exit();
}


void
queue_epilogue
(
 uint64_t queue_id
)
{
  // prevent self a sample interrupt while gathering calling context
  hpcrun_safe_enter();

  queue_map_entry_t *entry = queue_map_lookup(queue_id);
  queue_node_t *queue_node = queue_map_entry_queue_node_get(entry);

  queue_node_free_helper(&queue_node_free_list, queue_node);
  queue_map_delete(queue_id);

  hpcrun_safe_exit();
}


void
kernel_prologue
(
 uint64_t kernelexec_id
)
{
  // prevent self a sample interrupt while gathering calling context
  hpcrun_safe_enter();

  ucontext_t context;
  getcontext(&context);
  int skip_inner = 1; // what value should be passed here?
  cct_node_t *cct_node = hpcrun_sample_callpath(&context, cpu_idle_metric_id, (cct_metric_data_t){.i = 0}, skip_inner /*skipInner */ , 1 /*isSync */, NULL ).sample_node;

  create_and_insert_kernel_entry(kernelexec_id, cct_node);
  atomic_fetch_add(&g_unfinished_kernels, 1L);

  hpcrun_safe_exit();
}


// kernel_start and kernel_end should be in seconds
void
kernel_epilogue
(
 uint64_t kernelexec_id,
 unsigned long kernel_start,
 unsigned long kernel_end
)
{
  // prevent self a sample interrupt while gathering calling context
  hpcrun_safe_enter();

  kernel_map_entry_t *e_entry = kernel_map_lookup(kernelexec_id);
  if (!e_entry) {
    // it is possible that another callback for the same kernel execution triggered a kernel_epilogue before this
    // if so, it could have deleted its corresponding entry from kernel_map. If so, return
    hpcrun_safe_exit();
    return;
  }
	kernel_node_t *kernel_node = kernel_map_entry_kernel_node_get(e_entry);
	kernel_node->kernel_start_time = kernel_start;
	kernel_node->kernel_end_time = kernel_end;

	record_latest_kernel_end_time(kernel_node->kernel_end_time);
	add_kernel_to_completed_list(kernel_node);
	atomic_fetch_add(&g_unfinished_kernels, -1L);

  hpcrun_safe_exit();
}


void
sync_prologue
(
 uint64_t queue_id
)
{
  // prevent self a sample interrupt while gathering calling context
  hpcrun_safe_enter();

  struct timespec sync_start;
  clock_gettime(CLOCK_REALTIME, &sync_start); // get current time

  atomic_fetch_add(&g_num_threads_at_sync, 1L);

  // getting the queue node in splay-tree associated with the queue. We will need it for attributing CPU_IDLE_CAUSE
  queue_map_entry_t *entry = queue_map_lookup(queue_id);
  queue_node_t *queue_node = queue_map_entry_queue_node_get(entry);

  ucontext_t context;
  getcontext(&context);
  int skip_inner = 1; // what value should be passed here?
  cct_node_t *cpu_cct_node = hpcrun_sample_callpath(&context, cpu_idle_metric_id, (cct_metric_data_t){.i = 0}, skip_inner /*skipInner */ , 1 /*isSync */, NULL ).sample_node;

  queue_node->cpu_idle_cct = cpu_cct_node; // we may need to remove hpcrun functions from the stackframe of the cct
  queue_node->cpu_sync_start_time = &sync_start;

  hpcrun_safe_exit();
}


kernel_id_t
sync_epilogue
(
 uint64_t queue_id
)
{
  // prevent self a sample interrupt while gathering calling context
  hpcrun_safe_enter();

  struct timespec sync_end;
  clock_gettime(CLOCK_REALTIME, &sync_end); // get current time

  queue_map_entry_t *entry = queue_map_lookup(queue_id);
  queue_node_t *queue_node = queue_map_entry_queue_node_get(entry);
  cct_node_t *cpu_cct_node = queue_node->cpu_idle_cct;
  struct timespec sync_start = *(queue_node->cpu_sync_start_time);

	// converting nsec to sec
  double nsec_to_sec = pow(10,-9);
  double sync_start_sec = sync_start.tv_sec + sync_start.tv_nsec * nsec_to_sec;
  double sync_end_sec = sync_end.tv_sec + sync_end.tv_nsec * nsec_to_sec;
  attributing_cpu_idle_metric_at_sync_epilogue(cpu_cct_node, sync_start_sec, sync_end_sec);
  kernel_id_t processed_ids = attributing_cpu_idle_cause_metric_at_sync_epilogue(sync_start_sec, sync_end_sec);

  queue_node->cpu_idle_cct = NULL;
  queue_node->cpu_sync_start_time = NULL;
  atomic_fetch_add(&g_num_threads_at_sync, -1L);

  hpcrun_safe_exit();
	return processed_ids;
}



////////////////////////////////////////////////
// CPU-GPU blame shift itimer callback interface
////////////////////////////////////////////////

void
gpu_idle_blame
(
 void* dc,
 int metric_id,
 cct_node_t* node,
 int metric_dc
)
{
  metric_desc_t* metric_desc = hpcrun_id2metric(metric_id);

  // Only blame shift idleness for time metric.
  if ( !metric_desc->properties.time )
    return;

  uint64_t cur_time_us = 0;
  int ret = time_getTimeReal(&cur_time_us);
  if (ret != 0) {
    EMSG("time_getTimeReal (clock_gettime) failed!");
    monitor_real_abort();
  }

  // metric_incr is in microseconds. converting to sec
  double msec_to_sec = pow(10,-6);
  double metric_incr = cur_time_us - TD_GET(last_time_us);
  metric_incr *= msec_to_sec;

  spinlock_lock(&itimer_blame_lock);

  uint32_t num_unfinished_kernels = get_count_of_unfinished_kernels();
  if (num_unfinished_kernels == 0) {
		cct_metric_data_increment(gpu_idle_metric_id, node, (cct_metric_data_t) {.i = metric_incr});
	}

  spinlock_unlock(&itimer_blame_lock);
}

#endif	// ENABLE_OPENCL
