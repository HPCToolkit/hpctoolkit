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
// Copyright ((c)) 2002-2024, Rice University
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

#define _GNU_SOURCE

#include <papi.h>



//******************************************************************************
// local includes
//******************************************************************************

#include "papi-metric-collector.h"
#include "../../../gpu-activity-channel.h"                            // gpu_activity_channel_get
#include "../../../gpu-operation-multiplexer.h"                       // gpu_operation_multiplexer_push
#include "../../../../memory/hpcrun-malloc.h"                                // hpcrun_malloc_safe
#include "papi-kernel-map.h"



//******************************************************************************
// local data
//******************************************************************************

#define numMetrics 2
#define ACTIVE_INDEX 0
#define STALL_INDEX 1
#define MAX_STR_LEN     128

static int my_event_set = PAPI_NULL;
static char const *metric_name[MAX_STR_LEN] = {"ComputeBasic.EuActive", "ComputeBasic.EuStall"};
static papi_kernel_node_t *kernel_node_free_list = NULL;



//******************************************************************************
// private operations
//******************************************************************************

static papi_kernel_node_t*
kernel_node_alloc_helper
(
 papi_kernel_node_t **free_list
)
{
  papi_kernel_node_t *first = *free_list;

  if (first) {
    *free_list = first->next;
  } else {
    first = (papi_kernel_node_t *) hpcrun_malloc_safe(sizeof(papi_kernel_node_t));
  }
  memset(first, 0, sizeof(papi_kernel_node_t));
  return first;
}


static void
kernel_node_free_helper
(
 papi_kernel_node_t **free_list,
 papi_kernel_node_t *node
)
{
  node->next = *free_list;
  *free_list = node;
}


static void
attribute_gpu_utilization_counters_to_kernel
(
 cct_node_t *kernel_cct,
 gpu_activity_channel_t *kernel_activity_channel,
 long long *prev_values,
 long long *current_values
)
{
  gpu_activity_t ga;
  gpu_activity_t *ga_ptr = &ga;
  ga_ptr->kind = GPU_ACTIVITY_INTEL_GPU_UTILIZATION;
  ga_ptr->cct_node = kernel_cct;

  uint8_t kernel_eu_active = 0, kernel_eu_stall = 0;
  int16_t active_delta = current_values[ACTIVE_INDEX] - prev_values[ACTIVE_INDEX];
  int16_t stall_delta = current_values[STALL_INDEX] - prev_values[STALL_INDEX];
  if (active_delta > 0) kernel_eu_active = active_delta;
  if (stall_delta > 0) kernel_eu_stall = stall_delta;

  uint8_t kernel_eu_idle = 100 - (kernel_eu_active + kernel_eu_stall);
  ga_ptr->details.gpu_utilization_info = (gpu_utilization_t) { .active = kernel_eu_active,
                                            .stalled = kernel_eu_stall,
                                            .idle = kernel_eu_idle};
  printf("%s: %d, %s: %d, %s: %d\n", metric_name[ACTIVE_INDEX], kernel_eu_active,
                                     metric_name[STALL_INDEX], kernel_eu_stall,
                                     "ComputeBasic.EuIdle", kernel_eu_idle);
  cstack_ptr_set(&(ga_ptr->next), 0);
  gpu_operation_multiplexer_push(kernel_activity_channel, NULL, ga_ptr);
}



//******************************************************************************
// interface operations
//******************************************************************************


void
intel_papi_setup
(
 void
)
{
  my_event_set = PAPI_NULL;
  int ret=PAPI_library_init(PAPI_VER_CURRENT);
  if (ret!=PAPI_VER_CURRENT) {
    hpcrun_abort("Failure: PAPI_library_init.Return code = %d (%d)",
                   ret, PAPI_strerror(ret));
  }

  ret=PAPI_create_eventset(&my_event_set);
  if (ret!=PAPI_OK) {
    hpcrun_abort("Failure: PAPI_create_eventset.Return code = %d (%d)",
                   ret, PAPI_strerror(ret));
  }

  for (int i=0; i<numMetrics; i++) {
    ret=PAPI_add_named_event(my_event_set, metric_name[i]);
    if (ret!=PAPI_OK) {
      hpcrun_abort("Failure: PAPI_add_named_event().Return code = %s (%d)",
                   ret, PAPI_strerror(ret));
    }
  }

  PAPI_reset(my_event_set);
  ret=PAPI_start(my_event_set);
  if (ret!=PAPI_OK) {
    hpcrun_abort("Failure: PAPI_start.Return code = %s (%d)",
                  ret, PAPI_strerror(ret));
  }
}


void
intel_papi_teardown
(
 void
)
{
  int ret=PAPI_stop(my_event_set, NULL);
  if (ret!=PAPI_OK) {
    hpcrun_abort("Failure: PAPI_stop.Return code = %s (%d)",
                  ret, PAPI_strerror(ret));
  }

  // some metric attributions were missing from .hpcrun files,
  // hence adding a flush operation
  atomic_bool wait;
  atomic_store(&wait, true);
  gpu_activity_t gpu_activity;
  memset(&gpu_activity, 0, sizeof(gpu_activity_t));

  gpu_activity.kind = GPU_ACTIVITY_FLUSH;
  gpu_activity.details.flush.wait = &wait;
  // instead of using channel from kernel_activity_channel (channel that ran the last kernek),
  // we directly calling gpu_activity_channel_get as this will be called from main thread
  gpu_operation_multiplexer_push(gpu_activity_channel_get(), NULL, &gpu_activity);

  // Wait until operations are drained
  // Operation channel is FIFO
  while (atomic_load(&wait)) {}
}


void
papi_metric_collection_at_kernel_start
(
 uint64_t kernelexec_id,
 cct_node_t *kernel_cct,
 gpu_activity_channel_t *activity_channel
)
{
  papi_kernel_node_t *kernel_node = kernel_node_alloc_helper(&kernel_node_free_list);
  kernel_node->kernel_id = kernelexec_id;
  kernel_node->cct = kernel_cct;
  kernel_node->activity_channel = activity_channel;
  kernel_node->next = NULL;

  PAPI_read(my_event_set, kernel_node->prev_values);

  papi_kernel_map_insert(kernelexec_id, kernel_node);
}


void
papi_metric_collection_at_kernel_end
(
 uint64_t kernelexec_id
)
{
  long long collected_values[MAX_EVENTS];

  PAPI_read(my_event_set, collected_values);

  papi_kernel_map_entry_t *entry = papi_kernel_map_lookup(kernelexec_id);
  papi_kernel_node_t *kernel_node = papi_kernel_map_entry_kernel_node_get(entry);

  attribute_gpu_utilization_counters_to_kernel(kernel_node->cct, kernel_node->activity_channel,
                                               kernel_node->prev_values, collected_values);

  papi_kernel_map_delete(kernelexec_id);
  kernel_node_free_helper(&kernel_node_free_list, kernel_node);
}
