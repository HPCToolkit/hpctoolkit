// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2022, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

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



//******************************************************************************
// local data
//******************************************************************************

static spinlock_t incomplete_kernel_list_lock = SPINLOCK_UNLOCKED;
static bool hpcrun_complete = false;
static kernel_node_t* incomplete_kernel_list_head = NULL;
static kernel_node_t* incomplete_kernel_list_tail = NULL;
static kernel_node_t *kernel_node_free_list = NULL;
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
