#ifdef ENABLE_OPENCL

//******************************************************************************
// system includes
//******************************************************************************

#include <monitor.h>                          // monitor_real_abort
#include <stdbool.h>                          // bool
#include <ucontext.h>                         // getcontext



//******************************************************************************
// local includes
//******************************************************************************

#include "opencl-event-map.h"                 // event_node_t
#include "opencl-queue-map.h"                 // queue_node_t
#include "opencl-blame-helper.h"              // calculate_blame_for_active_kernels

#include <hpcrun/cct/cct.h>                   // cct_node_t
#include <hpcrun/memory/mmap.h>               // hpcrun_mmap_anon
#include <hpcrun/safe-sampling.h>             // hpcrun_safe_enter, hpcrun_safe_exit
#include <hpcrun/sample_event.h>              // hpcrun_sample_callpath

#include <lib/prof-lean/hpcrun-opencl.h>      // CL_SUCCESS, cl_event, etc
#include <lib/prof-lean/spinlock.h>           // spinlock_t, SPINLOCK_UNLOCKED
#include <lib/prof-lean/stdatomic.h>          // atomic_fetch_add
#include <lib/support-lean/timer.h>           // time_getTimeReal



//******************************************************************************
// macros
//******************************************************************************

#define	NS_IN_SEC 1000000000

#define NEXT(node) node->next



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
static _Atomic(uint32_t) g_unfinished_queues = { 0 };
static _Atomic(uint32_t) g_unfinished_kernels = { 0 };

static bool shared_blaming = true;

_Atomic(unsigned long) latest_kernel_end_time = { 0 };

static _Atomic(event_node_t*) completed_kernel_list_head = { NULL };

static queue_node_t *queue_node_free_list = NULL;
static event_node_t *event_node_free_list = NULL;
static epoch_t *epoch_free_list = NULL;



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
    *free_list = NEXT(first);
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
  NEXT(node) = *free_list;
  *free_list = node;
}


static event_node_t*
event_node_alloc_helper
(
 event_node_t **free_list
)
{
  event_node_t *first = *free_list; 

  if (first) { 
    *free_list = atomic_load(&first->next);
  } else {
    first = (event_node_t *) hpcrun_malloc_safe(sizeof(event_node_t));
  }
  memset(first, 0, sizeof(event_node_t));
  return first;
}


static void
event_node_free_helper
(
 event_node_t **free_list, 
 event_node_t *node 
)
{
  atomic_store(&node->next, *free_list);
  *free_list = node;
}


static epoch_t*
epoch_alloc_helper
(
 epoch_t **free_list
)
{
  epoch_t *first = *free_list; 

  if (first) { 
    *free_list = NEXT(first);
  } else {
    first = (epoch_t *) hpcrun_malloc_safe(sizeof(epoch_t));
  }

  memset(first, 0, sizeof(epoch_t));
  return first;
}


static void
epoch_free_helper
(
 epoch_t **free_list, 
 epoch_t *node 
)
{
  NEXT(node) = *free_list;
  *free_list = node;
}


//TODO: review this function
// Initialize hpcrun core_profile_trace_data for a new queue
static inline core_profile_trace_data_t*
hpcrun_queue_data_alloc_init
(
 int id
)
{
  core_profile_trace_data_t *st = hpcrun_mmap_anon(sizeof(core_profile_trace_data_t));
  // FIXME: revisit to perform this memstore operation appropriately.
  //memstore = td->memstore;
  //memset(st, 0xfe, sizeof(core_profile_trace_data_t));
  memset(st, 0, sizeof(core_profile_trace_data_t));
  //td->memstore = memstore;
  //hpcrun_make_memstore(&td->memstore, is_child);
  st->id = id;
  st->epoch = epoch_alloc_helper(&epoch_free_list);
  st->epoch->csdata_ctxt = copy_thr_ctxt(TD_GET(core_profile_trace_data.epoch)->csdata.ctxt); //copy_thr_ctxt(thr_ctxt);
  hpcrun_cct_bundle_init(&(st->epoch->csdata), (st->epoch->csdata).ctxt);
  st->epoch->loadmap = hpcrun_getLoadmap();
  st->epoch->next  = NULL;
  hpcrun_cct2metrics_init(&(st->cct2metrics_map)); //this just does st->map = NULL;

  st->trace_min_time_us = 0;
  st->trace_max_time_us = 0;
  st->hpcrun_file  = NULL;

  return st;
}


static void
add_queue_node_to_splay_tree
(
 cl_command_queue queue
)
{
  uint64_t queue_id = (uint64_t)queue;
  queue_node_t *node = queue_node_alloc_helper(&queue_node_free_list);

  node->st = hpcrun_queue_data_alloc_init(queue_id);	
  node->next = NULL;

  queue_map_insert(queue_id, node);
}


//TODO: review this function
static cct_node_t*
queue_duplicate_cpu_node
(
 core_profile_trace_data_t *st,
 ucontext_t *context,
 cct_node_t *node
)
{
  cct_bundle_t* cct= &(st->epoch->csdata);
  cct_node_t * tmp_root = cct->tree_root;
  hpcrun_cct_insert_path(&tmp_root, node);
  return tmp_root;
}


// Insert a new event node corresponding to a kernel in a queue
static void
create_and_insert_event
(
 uint64_t queue_id,
 queue_node_t *queue_node,
 cl_event event,
 cct_node_t *launcher_cct,
 cct_node_t *queue_launcher_cct
)
{
  event_node_t *event_node = event_node_alloc_helper(&event_node_free_list);
  event_node->event = event;
  event_node->queue_launcher_cct = queue_launcher_cct;
  event_node->launcher_cct = launcher_cct;
  atomic_init(&event_node->next, NULL);
  uint64_t event_id = (uint64_t) event;
  event_map_insert(event_id, event_node);
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
 event_node_t *event_node
)
{
try_again: ;
           event_node_t *current_head = atomic_load(&completed_kernel_list_head);
           atomic_store(&event_node->next, current_head);
           if (!atomic_compare_exchange_strong(&completed_kernel_list_head, &current_head, event_node)) {
             goto try_again;
           }
}


static void
attributing_cpu_idle_metric_at_sync_epilogue
(
 cct_node_t *cpu_cct_node,
 long sync_start,
 long sync_end
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


static void
attributing_cpu_idle_cause_metric_at_sync_epilogue
(
 cl_command_queue queue,
 long sync_start,
 long sync_end
)
{
  event_node_t *private_completed_kernel_head = atomic_load(&completed_kernel_list_head);
  if (private_completed_kernel_head == NULL) {
    // no kernels to attribute idle blame, return
    return;
  }
  event_node_t *null_ptr = NULL;
  if (atomic_compare_exchange_strong(&completed_kernel_list_head, &private_completed_kernel_head, null_ptr)) {
    // exchange successful, proceed with metric attribution	

    calculate_blame_for_active_kernels(private_completed_kernel_head, sync_start, sync_end);
    event_node_t *curr_event = private_completed_kernel_head;
    event_node_t *next;
    uint64_t event_id;
    while (curr_event) {
      cct_metric_data_increment(cpu_idle_cause_metric_id, curr_event->launcher_cct,
          (cct_metric_data_t) {.r = curr_event->cpu_idle_blame});
      event_id = (uint64_t) curr_event->event;
      next = atomic_load(&curr_event->next);
      event_map_delete(event_id);
      clReleaseEvent(curr_event->event);
      event_node_free_helper(&event_node_free_list, curr_event);
      curr_event = next;
    }

  } else {
    // some other sync block could have attributed its idleless blame, return
    return;
  }
}


static uint32_t
get_count_of_unfinished_queues
(
 void
)
{
  return atomic_load(&g_unfinished_queues);
}



//******************************************************************************
// interface operations
//******************************************************************************

void
queue_prologue
(
 cl_command_queue queue
)
{
  // prevent self a sample interrupt while gathering calling context
  hpcrun_safe_enter(); 

  add_queue_node_to_splay_tree(queue);
  atomic_fetch_add(&g_unfinished_queues, 1L);

  hpcrun_safe_exit();
}


void
queue_epilogue
(
 cl_command_queue queue
)
{
  // prevent self a sample interrupt while gathering calling context
  hpcrun_safe_enter(); 

  uint64_t queue_id = (uint64_t) queue;
  queue_map_entry_t *entry = queue_map_lookup(queue_id);
  queue_node_t *queue_node = queue_map_entry_queue_node_get(entry);

  epoch_free_helper(&epoch_free_list, queue_node->st->epoch);
  queue_node_free_helper(&queue_node_free_list, queue_node);
  queue_map_delete(queue_id);
  atomic_fetch_add(&g_unfinished_queues, -1L);

  hpcrun_safe_exit();
}


void
kernel_prologue
(
 cl_event event,
 cl_command_queue queue
)
{
  // prevent self a sample interrupt while gathering calling context
  hpcrun_safe_enter(); 

  // increment the reference count for the event
  clRetainEvent(event);

  ucontext_t context;
  getcontext(&context);
  int skip_inner = 1; // what value should be passed here?
  cct_node_t *cct_node = hpcrun_sample_callpath(&context, cpu_idle_metric_id, (cct_metric_data_t){.i = 0}, skip_inner /*skipInner */ , 1 /*isSync */, NULL ).sample_node;

  uint64_t queue_id = (uint64_t) queue;
  queue_map_entry_t *entry = queue_map_lookup(queue_id);
  queue_node_t *queue_node = queue_map_entry_queue_node_get(entry);
  // NULL checks need to be added for entry and queue_node

  cct_node_t *queue_cct = queue_duplicate_cpu_node(queue_node->st, &context, cct_node);
  create_and_insert_event(queue_id, queue_node, event, cct_node, queue_cct);
  atomic_fetch_add(&g_unfinished_kernels, 1L);

  hpcrun_safe_exit();
}


void
kernel_epilogue
(
 cl_event event,
 cl_command_queue queue
)
{
  // prevent self a sample interrupt while gathering calling context
  hpcrun_safe_enter(); 

  uint64_t event_id = (uint64_t) event;
  event_map_entry_t *e_entry = event_map_lookup(event_id);
  if (!e_entry) {
    // it is possible that another callback for the same kernel event triggered a kernel_epilogue before this
    // if so, it could have deleted its corresponding entry from event map. If so, return
    return;
  }
  event_node_t *event_node = event_map_entry_event_node_get(e_entry);

  cl_int err_cl = clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &(event_node->event_end_time), NULL);

  if (err_cl == CL_SUCCESS) {
		atomic_fetch_add(&g_unfinished_kernels, -1L);

    float elapsedTime;
    err_cl = clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START,	sizeof(cl_ulong), &(event_node->event_start_time), NULL);

    if (err_cl != CL_SUCCESS) {
      EMSG("clGetEventProfilingInfo failed");
    }
    // Just to verify that this is a valid profiling value. 
    elapsedTime = event_node->event_end_time - event_node->event_start_time; 
    assert(elapsedTime > 0);

    record_latest_kernel_end_time(event_node->event_end_time);

    // Add the kernel execution time to the gpu_time_metric_id
    cct_metric_data_increment(gpu_time_metric_id, event_node->launcher_cct, (cct_metric_data_t) {
        .i = (event_node->event_end_time - event_node->event_start_time)});

    add_kernel_to_completed_list(event_node);
  } else {
    EMSG("clGetEventProfilingInfo failed");
  }

  uint64_t queue_id = (uint64_t) queue;
  queue_map_entry_t *q_entry = queue_map_lookup(queue_id);
  queue_node_t *queue_node = queue_map_entry_queue_node_get(q_entry);

  hpcrun_safe_exit();
}


void
sync_prologue
(
 cl_command_queue queue
)
{
  // prevent self a sample interrupt while gathering calling context
  hpcrun_safe_enter(); 

  struct timespec sync_start;
  clock_gettime(CLOCK_MONOTONIC_RAW, &sync_start); // get current time

  atomic_fetch_add(&g_num_threads_at_sync, 1L);

  // getting the queue node in splay-tree associated with the queue. We will need it for attributing CPU_IDLE_CAUSE
  uint64_t queue_id = (uint64_t) queue;
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


void
sync_epilogue
(
 cl_command_queue queue
)
{
  // prevent self a sample interrupt while gathering calling context
  hpcrun_safe_enter(); 

  struct timespec sync_end;
  clock_gettime(CLOCK_MONOTONIC_RAW, &sync_end); // get current time

  uint64_t queue_id = (uint64_t) queue;
  queue_map_entry_t *entry = queue_map_lookup(queue_id);
  queue_node_t *queue_node = queue_map_entry_queue_node_get(entry);
  cct_node_t *cpu_cct_node = queue_node->cpu_idle_cct;
  struct timespec sync_start = *(queue_node->cpu_sync_start_time);

  long sync_start_nsec = sync_start.tv_sec*NS_IN_SEC + sync_start.tv_nsec;
  long sync_end_nsec = sync_end.tv_sec*NS_IN_SEC + sync_end.tv_nsec;
  attributing_cpu_idle_metric_at_sync_epilogue(cpu_cct_node, sync_start_nsec, sync_end_nsec);	
  attributing_cpu_idle_cause_metric_at_sync_epilogue(queue, sync_start_nsec, sync_end_nsec);	

  queue_node->cpu_idle_cct = NULL;
  queue_node->cpu_sync_start_time = NULL;
  atomic_fetch_add(&g_num_threads_at_sync, -1L);

  hpcrun_safe_exit();
}



////////////////////////////////////////////////
// CPU-GPU blame shift interface
////////////////////////////////////////////////

void
opencl_gpu_blame_shifter
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
  uint64_t metric_incr = cur_time_us - TD_GET(last_time_us);

  spinlock_lock(&itimer_blame_lock);

  uint32_t num_unfinished_queues = get_count_of_unfinished_queues();
  if (num_unfinished_queues == 0) {
    if(shared_blaming) {
      if (atomic_load(&g_unfinished_kernels) == 0) {
        cct_metric_data_increment(gpu_idle_metric_id, node, (cct_metric_data_t) {.i = metric_incr});
      }
    } else {
      cct_metric_data_increment(gpu_idle_metric_id, node, (cct_metric_data_t) {.i = metric_incr});
    }

  }
  spinlock_unlock(&itimer_blame_lock);
}

#endif	// ENABLE_OPENCL
