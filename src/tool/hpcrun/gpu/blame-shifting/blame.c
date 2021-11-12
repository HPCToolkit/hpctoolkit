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

#include <hpcrun/gpu-monitors.h>              // gpu_monitors_apply, gpu_monitor_type_enter
#include <hpcrun/cct/cct.h>                   // cct_node_t
#include <hpcrun/gpu/gpu-activity.h>          // gpu_activity_t
#include <hpcrun/gpu/gpu-metrics.h>           // gpu_metrics_attribute
#include <hpcrun/gpu/gpu-application-thread-api.h>  // gpu_application_thread_correlation_callback
#include <hpcrun/memory/hpcrun-malloc.h>      // hpcrun_malloc_safe
#include <hpcrun/safe-sampling.h>             // hpcrun_safe_enter, hpcrun_safe_exit

#include <lib/prof-lean/spinlock.h>           // spinlock_t, SPINLOCK_UNLOCKED
#include <lib/prof-lean/stdatomic.h>          // atomic_fetch_add
#include <lib/support-lean/timer.h>           // time_getTimeReal



//******************************************************************************
// local data
//******************************************************************************

//int gpu_idle_metric_id;

// lock for blame-shifting activities
static spinlock_t itimer_blame_lock = SPINLOCK_UNLOCKED;

static _Atomic(uint64_t) g_num_threads_at_sync = { 0 };
static _Atomic(uint32_t) g_unfinished_kernels = { 0 };

_Atomic(unsigned long) latest_kernel_end_time = { 0 }; // in nanoseconds

static kernel_node_t* completed_kernel_list_head = NULL;
static kernel_node_t* incomplete_kernel_list_head = NULL;
static kernel_node_t* incomplete_kernel_list_tail = NULL;

static queue_node_t *queue_node_free_list = NULL;
static kernel_node_t *kernel_node_free_list = NULL;
static cct_node_linkedlist_t *cct_list_node_free_list = NULL;

static uint32_t cct_list_alloc_count = 0;
static spinlock_t completed_kernel_list_lock = SPINLOCK_UNLOCKED;
static spinlock_t incomplete_kernel_list_lock = SPINLOCK_UNLOCKED;
static spinlock_t queue_node_lock = SPINLOCK_UNLOCKED;

__thread bool gpu_utilization_enabled = false;



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
    *free_list = first->next;
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
  spinlock_lock(&queue_node_lock);
  node->next =  *free_list;
  *free_list = node;
  spinlock_unlock(&queue_node_lock);
}


static kernel_node_t*
kernel_node_alloc_helper
(
 kernel_node_t **free_list
)
{
  kernel_node_t *first = *free_list;

  if (first) {
    *free_list = first->next;
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
  node->next = *free_list;
  *free_list = node;
}


static cct_node_linkedlist_t*
cct_list_node_alloc_helper
(
 cct_node_linkedlist_t **free_list
)
{
  cct_list_alloc_count++;
  cct_node_linkedlist_t *first = *free_list;

  if (first) {
    *free_list = atomic_load(&first->next);
  } else {
    first = (cct_node_linkedlist_t *) hpcrun_malloc_safe(sizeof(cct_node_linkedlist_t));
  }
  memset(first, 0, sizeof(cct_node_linkedlist_t));
  return first;
}


void
cct_list_node_free_helper
(
 cct_node_linkedlist_t *node
)
{
  cct_node_linkedlist_t **free_list = &cct_list_node_free_list;
  cct_node_linkedlist_t *incoming_list_head = node;
  cct_node_linkedlist_t *incoming_list_tail = node;
  cct_node_linkedlist_t *next = atomic_load(&incoming_list_tail->next);

  // goto end of incoming_list
  while (next) {
    incoming_list_tail = next;
    next = atomic_load(&incoming_list_tail->next);
  }

  cct_node_linkedlist_t *old_list_head = *free_list;
  // swap current head with incoming head
  *free_list = incoming_list_head;

  // incoming_list->end->next = old_head
  atomic_store(&incoming_list_tail->next, old_list_head);
}


static void
create_activity_object
(
 gpu_activity_t *ga,
 cct_node_t *cct_node,
 gpu_blame_shift_t *activity
)
{
  ga->kind = GPU_ACTIVITY_BLAME_SHIFT;
  ga->cct_node = cct_node;
  ga->details.blame_shift.cpu_idle_time = activity->cpu_idle_time;
  ga->details.blame_shift.gpu_idle_cause_time = activity->gpu_idle_cause_time;
  ga->details.blame_shift.cpu_idle_cause_time = activity->cpu_idle_cause_time;
}


static void
record_blame_shift_metrics
(
 cct_node_t *cct_node,
 gpu_blame_shift_t *bs
)
{
  gpu_activity_t ga;
  create_activity_object(&ga, cct_node, bs);
  gpu_metrics_attribute(&ga);
}


static void
add_queue_node_to_splay_tree
(
 uint64_t queue_id
)
{
  queue_node_t *node = queue_node_alloc_helper(&queue_node_free_list);
  node->next = NULL;
  queue_map_insert(queue_id, node);
}


static void
add_kernel_to_incomplete_list
(
 kernel_node_t *kernel_node
)
{
  spinlock_lock(&incomplete_kernel_list_lock);
  kernel_node_t *current_tail = incomplete_kernel_list_tail;
  incomplete_kernel_list_tail = kernel_node;
  if (current_tail != NULL) {
    current_tail->next = kernel_node;
  } else {
    incomplete_kernel_list_head = kernel_node;
  }
  spinlock_unlock(&incomplete_kernel_list_lock);
}


static void
remove_kernel_from_incomplete_list
(
 kernel_node_t *kernel_node
)
{
  spinlock_lock(&incomplete_kernel_list_lock);
  kernel_node_t *curr = incomplete_kernel_list_head, *prev = NULL, *next = NULL;
  while (curr) {
    next = curr->next;
    if (kernel_node == curr) {
      if (prev) {
        if (!next) {
          incomplete_kernel_list_tail = NULL;
        }
        prev->next = next;
      } else {
        if (!next) {
          incomplete_kernel_list_tail = NULL;
        }
        incomplete_kernel_list_head = next;
      }
    }
    curr = curr->next;
  }
  spinlock_unlock(&incomplete_kernel_list_lock);
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
  kernel_node->next = NULL;
  kernel_map_insert(kernelexec_id, kernel_node);
  add_kernel_to_incomplete_list(kernel_node);
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
  spinlock_lock(&completed_kernel_list_lock);
  kernel_node->next = completed_kernel_list_head;
  completed_kernel_list_head = kernel_node;
  spinlock_unlock(&completed_kernel_list_lock);
}


static void
attributing_cpu_idle_metric_at_sync_epilogue
(
 cct_node_t *cpu_cct_node,
 unsigned long sync_start,
 unsigned long sync_end
)
{
  // attributing cpu_idle time for the min of (sync_end - sync_start, sync_end - last_kernel_end)
  uint64_t last_kernel_end_time = atomic_load(&latest_kernel_end_time);

  uint64_t cpu_idle_time = ((sync_end - last_kernel_end_time) < (sync_end - sync_start)) ?
    (sync_end - last_kernel_end_time) :	(sync_end  - sync_start);
  // converting nsec to sec
  double nsec_to_sec = pow(10,-9);
  double cpu_idle_time_in_sec = cpu_idle_time * nsec_to_sec;

  gpu_blame_shift_t bs = {cpu_idle_time_in_sec, 0, 0};
  record_blame_shift_metrics(cpu_cct_node, &bs);
}


// this function returns a list of kernel nodes that have been completely processed
// uses may be do free resources related to the processed kernels e.g in opencl, we can call clReleaseEvent
static kernel_id_t
attributing_cpu_idle_cause_metric_at_sync_epilogue
(
 unsigned long sync_start,
 unsigned long sync_end
)
{
  kernel_id_t processed_ids = {0, NULL};
  spinlock_lock(&completed_kernel_list_lock);
  kernel_node_t *private_completed_kernel_head = completed_kernel_list_head;
  if (private_completed_kernel_head == NULL) {
    // no kernels to attribute idle blame, return
    spinlock_unlock(&completed_kernel_list_lock);
    return processed_ids;
  }
  kernel_node_t *null_ptr = NULL;
  if (completed_kernel_list_head == private_completed_kernel_head) {
    completed_kernel_list_head = null_ptr;
    spinlock_unlock(&completed_kernel_list_lock);
    // exchange successful, proceed with metric attribution

    calculate_blame_for_active_kernels(private_completed_kernel_head, sync_start, sync_end);
    kernel_node_t *curr = private_completed_kernel_head;
    kernel_node_t *next;
    long length = 0;
    while (curr) {
      length++;
      curr = curr->next;
    }

    uint64_t *id = hpcrun_malloc_safe(sizeof(uint64_t) * length);  // how to free/reuse array data?
    long i = 0;
    curr = private_completed_kernel_head;
    while (curr) {
      gpu_blame_shift_t bs = {0, 0, curr->cpu_idle_blame};
      record_blame_shift_metrics(curr->launcher_cct, &bs);

      next = curr->next;
      kernel_map_delete(curr->kernel_id);
      id[i++] = curr->kernel_id;
      kernel_node_free_helper(&kernel_node_free_list, curr);
      curr = next;
    }
    processed_ids.id = id;
    processed_ids.length = length;
  } else {
    spinlock_unlock(&completed_kernel_list_lock);
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


static void
accumulate_gpu_utilization_metrics_to_incomplete_kernels
(
 uint32_t num_unfinished_kernels
)
{
  // this function should be run only for Intel programs (unless the used runtime supports PAPI with active/stall metrics)
  int nodes_to_be_allocated = num_unfinished_kernels;
  cct_node_linkedlist_t* curr_c, *cct_list_of_incomplete_kernels, *new_node;
  cct_list_of_incomplete_kernels = cct_list_node_alloc_helper(&cct_list_node_free_list);
  curr_c = cct_list_of_incomplete_kernels;
  while (--nodes_to_be_allocated > 0) {
    new_node = cct_list_node_alloc_helper(&cct_list_node_free_list);
    atomic_store(&curr_c->next, new_node);
    curr_c = new_node;
  }

  // cct_node_t** cct_array_of_incomplete_kernels = (cct_node_t**) malloc(sizeof(cct_node_t*) * num_unfinished_kernels);  // this array needs to be freed
  spinlock_lock(&incomplete_kernel_list_lock);
  kernel_node_t *curr_k;
  curr_k = incomplete_kernel_list_head;

  /* We iterate over the list of unfinished kernels. For all kernels, we:
   * 1. add the cct_node for the kernel to an array of cct_nodes and accumulate the utilization metrics to that node
   * 2. delete the entry from the list if marked for deletion (i.e. the kernel has completed)
   */
  curr_c = cct_list_of_incomplete_kernels;
  while (curr_k) {
    curr_c->node = curr_k->launcher_cct;
    curr_c = atomic_load(&curr_c->next);
    curr_k = curr_k->next;
  }
  spinlock_unlock(&incomplete_kernel_list_lock);
  // gpu_monitors_apply(cct_array_of_incomplete_kernels, num_unfinished_kernels, gpu_monitor_type_enter);
  gpu_monitors_apply(cct_list_of_incomplete_kernels, num_unfinished_kernels, gpu_monitor_type_enter);
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
  cct_node_t *cct_node = gpu_application_thread_correlation_callback(0); // param is not used in the function

  create_and_insert_kernel_entry(kernelexec_id, cct_node);
  atomic_fetch_add(&g_unfinished_kernels, 1L);

  hpcrun_safe_exit();
}


// kernel_start and kernel_end should be in nanoseconds
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
  remove_kernel_from_incomplete_list(kernel_node);
  kernel_node->next = NULL;
  add_kernel_to_completed_list(kernel_node);
  atomic_fetch_add(&g_unfinished_kernels, -1L);
  hpcrun_safe_exit();
}


void
sync_prologue
(
 uint64_t queue_id,
 struct timespec sync_start
)
{
  // prevent self a sample interrupt while gathering calling context
  hpcrun_safe_enter();

  atomic_fetch_add(&g_num_threads_at_sync, 1L);

  // getting the queue node in splay-tree associated with the queue. We will need it for attributing CPU_IDLE_CAUSE
  queue_map_entry_t *entry = queue_map_lookup(queue_id);
  queue_node_t *queue_node = queue_map_entry_queue_node_get(entry);

  ucontext_t context;
  getcontext(&context);
  cct_node_t *cpu_cct_node = gpu_application_thread_correlation_callback(0); // param is not used in the function

  queue_node->cpu_idle_cct = cpu_cct_node; // we may need to remove hpcrun functions from the stackframe of the cct
  queue_node->cpu_sync_start_time = sync_start;

  hpcrun_safe_exit();
}


kernel_id_t
sync_epilogue
(
 uint64_t queue_id,
 struct timespec sync_end
)
{
  // prevent self a sample interrupt while gathering calling context
  hpcrun_safe_enter();

  queue_map_entry_t *entry = queue_map_lookup(queue_id);
  queue_node_t *queue_node = queue_map_entry_queue_node_get(entry);
  cct_node_t *cpu_cct_node = queue_node->cpu_idle_cct;
  struct timespec sync_start = queue_node->cpu_sync_start_time;

  // converting sec to nsec
  unsigned long sec_to_nsec = pow(10,9);
  unsigned long sync_start_nsec = sync_start.tv_sec * sec_to_nsec + sync_start.tv_nsec;
  unsigned long sync_end_nsec = sync_end.tv_sec * sec_to_nsec + sync_end.tv_nsec;
  attributing_cpu_idle_metric_at_sync_epilogue(cpu_cct_node, sync_start_nsec, sync_end_nsec);
  kernel_id_t processed_ids = attributing_cpu_idle_cause_metric_at_sync_epilogue(sync_start_nsec, sync_end_nsec);

  queue_node->cpu_idle_cct = NULL;
  // queue_node->cpu_sync_start_time = NULL;
  atomic_fetch_add(&g_num_threads_at_sync, -1L);

  hpcrun_safe_exit();
  return processed_ids;
}


void
gpu_blame_gpu_utilization_enable
(
 void
)
{
  gpu_utilization_enabled = true;
}



////////////////////////////////////////////////
// CPU-GPU blame shift itimer callback interface
////////////////////////////////////////////////

// how is this function called? which thread is monitored here?
// application helper threads are also attributed GPU_IDLE_CAUSE(threads at thread root of viewer)
// This is incorrect. Only program root should be getting these metrics
void
gpu_idle_blame
(
 void* dc,
 int metric_id,
 cct_node_t* node,
 int metric_dc
)
{
#if 1
  metric_desc_t* metric_desc = hpcrun_id2metric(metric_id);

  // Only blame shift idleness for time metric.
  if (!metric_desc->properties.time)
    return;

  spinlock_lock(&itimer_blame_lock);

  uint64_t cur_time_us = 0;
  int ret = time_getTimeCPU(&cur_time_us);
  if (ret != 0) {
    EMSG("time_getTimeCPU (clock_gettime) failed!");
    spinlock_unlock(&itimer_blame_lock);
    monitor_real_abort();
    return;
  }

  // metric_incr is in microseconds. converting to sec
  double msec_to_sec = pow(10,-6);
  double metric_incr = cur_time_us - TD_GET(last_time_us);
  metric_incr *= msec_to_sec;

  uint32_t num_unfinished_kernels = get_count_of_unfinished_kernels();
  if (num_unfinished_kernels == 0) {
    gpu_blame_shift_t bs = {0, metric_incr, 0};
    record_blame_shift_metrics(node, &bs);
  } else {
    if (gpu_utilization_enabled) {
      accumulate_gpu_utilization_metrics_to_incomplete_kernels(num_unfinished_kernels);
    }
  }

  spinlock_unlock(&itimer_blame_lock);
#endif
}

#endif	// ENABLE_OPENCL
