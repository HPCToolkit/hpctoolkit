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

#include <papi.h>



//******************************************************************************
// local includes
//******************************************************************************

#include "papi-metric-collector.h"
#include <hpcrun/gpu/gpu-activity-channel.h>                            // gpu_activity_channel_get
#include <hpcrun/gpu/gpu-operation-multiplexer.h>                       // gpu_operation_multiplexer_push



//******************************************************************************
// local data
//******************************************************************************

#define numMetrics 3
#define ACTIVE_INDEX 1
#define STALL_INDEX 2
#define MAX_STR_LEN     128
int my_event_set = PAPI_NULL;
char const *metric_name[MAX_STR_LEN] = {
           "ComputeBasic.GpuTime",      // TODO: remove this metric if unnecessary
           "ComputeBasic.EuActive",
           "ComputeBasic.EuStall"
};
cct_node_t *kernel_cct_node;
gpu_activity_channel_t *kernel_activity_channel;
long long prev_values[numMetrics];



//******************************************************************************
// private operations
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

  // some metric attrbutions were missing from .hpcrun files, 
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


static void
attribute_gpu_utilization_counters_to_kernel
(
 long long *current_values
)
{
  gpu_activity_t ga;
  gpu_activity_t *ga_ptr = &ga;
  ga_ptr->kind = GPU_ACTIVITY_INTEL_GPU_UTILIZATION;
  ga_ptr->cct_node = kernel_cct_node;

  uint8_t kernel_eu_active = current_values[ACTIVE_INDEX] - prev_values[ACTIVE_INDEX];
  uint8_t kernel_eu_stall  = current_values[STALL_INDEX] - prev_values[STALL_INDEX];
  uint8_t kernel_eu_idle = 100 - (kernel_eu_active + kernel_eu_stall);
  ga_ptr->details.gpu_utilization_info = (gpu_utilization_t) { .active = kernel_eu_active,
                                            .stalled = kernel_eu_stall,
                                            .idle = kernel_eu_idle};
  // printf("%s: %lld, %s: %lld, %s: %lld\n", metric_name[ACTIVE_INDEX], kernel_eu_active,
  //                                          metric_name[STALL_INDEX], kernel_eu_stall,
  //                                          "ComputeBasic.EuIdle", kernel_eu_idle);
  cstack_ptr_set(&(ga_ptr->next), 0);
  gpu_operation_multiplexer_push(kernel_activity_channel, NULL, ga_ptr);
}



//******************************************************************************
// interface operations
//******************************************************************************

void
papi_metric_collection_at_kernel_start
(
 cct_node_t *kernel_cct,
 gpu_activity_channel_t *activity_channel
)
{
  kernel_cct_node = kernel_cct;
  kernel_activity_channel = activity_channel;
  PAPI_read(my_event_set, prev_values);
}


void
papi_metric_collection_at_kernel_end
(
 void
)
{
  long long collected_values[MAX_EVENTS];

  PAPI_read(my_event_set, collected_values);
  attribute_gpu_utilization_counters_to_kernel(collected_values);
}
