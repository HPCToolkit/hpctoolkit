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
// Copyright ((c)) 2002-2020, Rice University
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

#include <assert.h>
#include <inttypes.h>



//******************************************************************************
// local includes
//******************************************************************************

#include <hpcrun/safe-sampling.h>
#include <hpcrun/gpu/gpu-activity.h>
#include <hpcrun/gpu/gpu-activity-channel.h>
#include <hpcrun/gpu/gpu-activity-process.h>
#include <hpcrun/gpu/gpu-correlation-channel.h>
#include <hpcrun/gpu/gpu-correlation-id-map.h>
#include <hpcrun/gpu/gpu-application-thread-api.h>
#include <hpcrun/gpu/gpu-metrics.h>
#include <hpcrun/gpu/gpu-monitoring-thread-api.h>
#include <hpcrun/gpu/gpu-op-placeholders.h>
#include <hpcrun/messages/messages.h>
#include <lib/prof-lean/stdatomic.h>
#include <lib/prof-lean/usec_time.h>

#include "opencl-api.h"
#include "opencl-activity-translate.h"
#include "opencl-memory-manager.h"



//******************************************************************************
// macros
//******************************************************************************

#define CPU_NANOTIME() (usec_time() * 1000)



//******************************************************************************
// local data
//******************************************************************************

static atomic_ullong opencl_pending_operations;



//******************************************************************************
// private operations
//******************************************************************************

static void
opencl_pending_operations_adjust
(
  int value
)
{
  atomic_fetch_add(&opencl_pending_operations, value);
}


static void
opencl_activity_completion_notify
(
  void
)
{
  gpu_monitoring_thread_activities_ready();
}


static void
opencl_activity_process
(
  cl_event event,
  void * user_data
)
{
  gpu_activity_t gpu_activity;
  opencl_activity_translate(&gpu_activity, event, user_data);
  gpu_activity_process(&gpu_activity);
}


static void
opencl_wait_for_pending_operations
(
  void
)
{
  ETMSG(CL, "pending operations: %lu", atomic_load(&opencl_pending_operations));
  while (atomic_load(&opencl_pending_operations) != 0);
}



//******************************************************************************
// interface operations
//******************************************************************************

void
opencl_subscriber_callback
(
  opencl_call type,
  uint64_t correlation_id
)
{
  opencl_pending_operations_adjust(1);
  gpu_op_placeholder_flags_t gpu_op_placeholder_flags = 0;
  gpu_op_ccts_t gpu_op_ccts;
  gpu_correlation_id_map_insert(correlation_id, correlation_id);
  cct_node_t *api_node = gpu_application_thread_correlation_callback(correlation_id);

  switch (type) {
    case memcpy_H2D:
      gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags, gpu_placeholder_type_copyin);
      break;
    case memcpy_D2H:
      gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags, gpu_placeholder_type_copyout);
      break;
    case kernel:
      gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags, gpu_placeholder_type_kernel);
      break;
    default:
      assert(0);
  }

  hpcrun_safe_enter();
  gpu_op_ccts_insert(api_node, &gpu_op_ccts, gpu_op_placeholder_flags);
  hpcrun_safe_exit();

  gpu_activity_channel_consume(gpu_metrics_attribute);	
  uint64_t cpu_submit_time = CPU_NANOTIME();
  gpu_correlation_channel_produce(correlation_id, &gpu_op_ccts, cpu_submit_time);
}


void
opencl_buffer_completion_callback
(
  cl_event event,
  cl_int event_command_exec_status,
  void *user_data
)
{
  cl_int complete_flag = CL_COMPLETE;
  opencl_object_t* o = (opencl_object_t*)user_data;
  cl_generic_callback_t* act_data;
  if (o->kind == OPENCL_KERNEL_CALLBACK) {
    act_data = (cl_generic_callback_t*) &(o->details.ker_cb);
  } else if (o->kind == OPENCL_MEMORY_CALLBACK) {
    act_data = (cl_generic_callback_t*) &(o->details.mem_cb);
  }
  uint64_t correlation_id = act_data->correlation_id;
  opencl_call type = act_data->type;

  if (event_command_exec_status == complete_flag) {
      gpu_correlation_id_map_entry_t *cid_map_entry = gpu_correlation_id_map_lookup(correlation_id);
    if (cid_map_entry == NULL) {
      ETMSG(CL, "completion callback was called before registration callback. type: %d, correlation: %"PRIu64 "", type, correlation_id);
    }
    ETMSG(CL, "completion type: %d, Correlation id: %"PRIu64 "", type, correlation_id);
    opencl_activity_completion_notify();
    opencl_activity_process(event, act_data);
  }
  if (o->isInternalClEvent) {
    clReleaseEvent(event);
  }
  opencl_free(o);
  opencl_pending_operations_adjust(-1);
}


void
opencl_api_initialize
(
  void
)
{
  atomic_store(&opencl_pending_operations, 0);
}


void
opencl_api_finalize
(
  void* args
)
{
  opencl_wait_for_pending_operations();
  gpu_application_thread_process_activities();
}
