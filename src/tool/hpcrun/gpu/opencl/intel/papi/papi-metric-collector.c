//******************************************************************************
// system includes
//******************************************************************************

#include <unistd.h>                                                     // usleep



//******************************************************************************
// local includes
//******************************************************************************

#include "papi-metric-collector.h"
#include <lib/prof-lean/spinlock.h>                                     // spinlock_t, SPINLOCK_UNLOCKED
#include <hpcrun/gpu-monitors.h>                                        // gpu_monitors_apply, gpu_monitor_type_enter
#include <hpcrun/memory/hpcrun-malloc.h>                                // hpcrun_malloc_safe
#include <hpcrun/gpu/opencl/opencl-api.h>                               // opencl_api_thread_finalize
#include <hpcrun/device-finalizers.h>                                   // device_finalizer_register



//******************************************************************************
// local data
//******************************************************************************

static spinlock_t incomplete_kernel_list_lock = SPINLOCK_UNLOCKED;
static bool hpcrun_complete = false;
static kernel_node_t* incomplete_kernel_list_head = NULL;
static kernel_node_t* incomplete_kernel_list_tail = NULL;
static kernel_node_t *kernel_node_free_list = NULL;
static device_finalizer_fn_entry_t device_finalizer_flush;
static _Atomic(uint32_t) g_unfinished_kernels = { 0 };



//******************************************************************************
// private operations
//******************************************************************************

static uint32_t
get_count_of_unfinished_kernels
(
 void
)
{
  return atomic_load(&g_unfinished_kernels);
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


static void
accumulate_gpu_utilization_metrics_to_incomplete_kernels
(
 void
)
{
  spinlock_lock(&incomplete_kernel_list_lock);
  uint32_t num_unfinished_kernels = get_count_of_unfinished_kernels();
  if (num_unfinished_kernels == 0) {
    spinlock_unlock(&incomplete_kernel_list_lock);
    return;
  }
  // this function should be run only for Intel programs (unless the used runtime supports PAPI with active/stall metrics)
  gpu_monitors_apply(incomplete_kernel_list_tail, num_unfinished_kernels, gpu_monitor_type_enter);
  spinlock_unlock(&incomplete_kernel_list_lock);
}



//******************************************************************************
// interface operations
//******************************************************************************

void
notify_gpu_util_thr_hpcrun_completion
(
 void
)
{
  if (!hpcrun_complete) {
    hpcrun_complete = true;
  }
}

void
add_kernel_to_incomplete_list
(
 kernel_node_t *kernel_node
)
{
  spinlock_lock(&incomplete_kernel_list_lock);
  atomic_fetch_add(&g_unfinished_kernels, 1L);
  // the reason for allocating a new kernel_node_t* instead of using the function parameter is
  // that the links (next pointer) for the parameter may get updated at the caller site. This will result in
  // loss of integrity of incomplete_kernel_list_head linked-list
  kernel_node_t *new_node = kernel_node_alloc_helper(&kernel_node_free_list);
  *new_node = *kernel_node;

  kernel_node_t *current_tail = incomplete_kernel_list_tail;
  incomplete_kernel_list_tail = new_node;
  if (current_tail != NULL) {
    current_tail->next = new_node;
  } else {
    incomplete_kernel_list_head = new_node;
  }
  spinlock_unlock(&incomplete_kernel_list_lock);
}


void
remove_kernel_from_incomplete_list
(
 kernel_node_t *kernel_node
)
{
  spinlock_lock(&incomplete_kernel_list_lock);
  kernel_node_t *curr = incomplete_kernel_list_head, *prev = NULL, *next = NULL;
  while (curr) {
    next = curr->next;
    if (kernel_node->kernel_id == curr->kernel_id) {
      if (prev) {
        if (!next) {
          incomplete_kernel_list_tail = prev;
        }
        prev->next = next;
      } else {
        if (!next) {
          incomplete_kernel_list_tail = NULL;
        }
        incomplete_kernel_list_head = next;
      }
      break;
    }
    prev = curr;
    curr = next;
  }
  kernel_node_free_helper(&kernel_node_free_list, curr);
  atomic_fetch_add(&g_unfinished_kernels, -1L);
  spinlock_unlock(&incomplete_kernel_list_lock);
}


void*
papi_metric_callback
(
 void *arg
)
{
  hpcrun_thread_init_mem_pool_once(0, NULL, false, true);
  while (!hpcrun_complete) {
    accumulate_gpu_utilization_metrics_to_incomplete_kernels();
    usleep(5000);  // sleep for 5000 microseconds i.e fire 200 times per second
  }
  opencl_api_thread_finalize(NULL, 0);
}
