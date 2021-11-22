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



//******************************************************************************
// local data
//******************************************************************************

static spinlock_t incomplete_kernel_list_lock = SPINLOCK_UNLOCKED;
static bool hpcrun_complete = false;
static kernel_node_t* incomplete_kernel_list_head = NULL;
static kernel_node_t* incomplete_kernel_list_tail = NULL;
static cct_node_linkedlist_t *cct_list_node_free_list = NULL;



//******************************************************************************
// private operations
//******************************************************************************

static cct_node_linkedlist_t*
cct_list_node_alloc_helper
(
 cct_node_linkedlist_t **free_list
)
{
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


void
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
  gpu_monitors_apply(cct_list_of_incomplete_kernels, num_unfinished_kernels, gpu_monitor_type_enter);
}



//******************************************************************************
// interface operations
//******************************************************************************

void
hpcrun_completed
(
 void
)
{
  hpcrun_complete = true;
}


void*
papi_metric_callback
(
 void *arg
)
{
  while (!hpcrun_complete) {
    uint32_t num_unfinished_kernels = get_count_of_unfinished_kernels();
    if (num_unfinished_kernels != 0) {
      accumulate_gpu_utilization_metrics_to_incomplete_kernels(num_unfinished_kernels);
    }
    usleep(5000);  // sleep for 5000 microseconds i.e fire 200 times per second
  }
}
