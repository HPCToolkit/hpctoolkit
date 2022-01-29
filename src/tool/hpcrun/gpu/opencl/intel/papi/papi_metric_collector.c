//******************************************************************************
// system includes
//******************************************************************************

#include <unistd.h>                                                     // usleep



//******************************************************************************
// local includes
//******************************************************************************

#include "papi_metric_collector.h"
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
static cct_node_linkedlist_t *cct_list_node_free_list = NULL;
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


static cct_node_linkedlist_t*
cct_list_node_alloc_helper
(
 cct_node_linkedlist_t **free_list
)
{
  cct_node_linkedlist_t *first = *free_list;

  if (first) {
    *free_list = first->next;
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
  cct_node_linkedlist_t *next = incoming_list_tail->next;
  // goto end of incoming_list
  while (next) {
    incoming_list_tail = next;
    next = incoming_list_tail->next;
  }

  cct_node_linkedlist_t *old_list_head = *free_list;
  // swap current head with incoming head
  *free_list = incoming_list_head;

  // incoming_list->end->next = old_head
  incoming_list_tail->next = old_list_head;
}


void
add_kernel_to_incomplete_list
(
 kernel_node_t *kernel_node
)
{
  spinlock_lock(&incomplete_kernel_list_lock);
  atomic_fetch_add(&g_unfinished_kernels, 1L);
  kernel_node_t *current_tail = incomplete_kernel_list_tail;
  incomplete_kernel_list_tail = kernel_node;
  if (current_tail != NULL) {
    current_tail->next = kernel_node;
  } else {
    incomplete_kernel_list_head = kernel_node;
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
    }
    prev = curr;
    curr = next;
  }
  atomic_fetch_add(&g_unfinished_kernels, -1L);
  spinlock_unlock(&incomplete_kernel_list_lock);
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
  uint32_t nodes_to_be_allocated = num_unfinished_kernels;
  cct_node_linkedlist_t* curr_c, *cct_list_of_incomplete_kernels, *new_node;
  cct_list_of_incomplete_kernels = cct_list_node_alloc_helper(&cct_list_node_free_list);
  curr_c = cct_list_of_incomplete_kernels;

  while (--nodes_to_be_allocated > 0) {
    new_node = cct_list_node_alloc_helper(&cct_list_node_free_list);
    curr_c->next = new_node;
    curr_c = new_node;
  }

  kernel_node_t *curr_k;
  curr_k = incomplete_kernel_list_head;

  /* We iterate over the list of unfinished kernels. For all kernels, we:
   * 1. add the cct_node for the kernel to an array of cct_nodes and accumulate the utilization metrics to that node
   * 2. delete the entry from the list if marked for deletion (i.e. the kernel has completed)
   */
  curr_c = cct_list_of_incomplete_kernels;
  while (curr_c && curr_k) {
    curr_c->node = curr_k->launcher_cct;
    curr_c->activity_channel = curr_k->activity_channel;
    curr_c = curr_c->next;
    curr_k = curr_k->next;
  }
  gpu_monitors_apply(cct_list_of_incomplete_kernels, num_unfinished_kernels, gpu_monitor_type_enter);
  cct_list_node_free_helper(cct_list_of_incomplete_kernels);
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
