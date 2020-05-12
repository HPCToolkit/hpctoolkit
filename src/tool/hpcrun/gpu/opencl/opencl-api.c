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
#include <inttypes.h> //PRIu64

//******************************************************************************
// local includes
//******************************************************************************
#include <hpcrun/gpu/gpu-activity.h> // gpu_activity_t 
#include <hpcrun/gpu/gpu-activity-process.h> //gpu_activity_process
#include <hpcrun/gpu/gpu-activity-channel.h> //gpu_activity_channel_consume
#include <hpcrun/gpu/gpu-correlation-channel.h> //gpu_correlation_channel_produce
#include <hpcrun/gpu/gpu-correlation-id-map.h> //gpu_correlation_id_map_insert
#include <hpcrun/gpu/gpu-application-thread-api.h> // gpu_application_thread_correlation_callback
#include <hpcrun/gpu/gpu-op-placeholders.h> // gpu_op_placeholder_flags_set, gpu_op_ccts_insert
#include <hpcrun/gpu/gpu-metrics.h> //gpu_metrics_attribute
#include <hpcrun/gpu/gpu-monitoring-thread-api.h> //gpu_monitoring_thread_activities_ready
#include <hpcrun/safe-sampling.h> //hpcrun_safe_enter, hpcrun_safe_exit 
#include <hpcrun/messages/messages.h> //ETMSG
#include <lib/prof-lean/usec_time.h> //usec_time
#include <lib/prof-lean/stdatomic.h> //

#include "opencl-api.h"
#include "opencl-activity-translate.h"

//******************************************************************************
// macros
//******************************************************************************
#define CPU_NANOTIME() (usec_time() * 1000)

//******************************************************************************
// type declarations
//******************************************************************************
static void opencl_pending_operations_adjust(int);
static void opencl_activity_completion_notify(void);
static void opencl_activity_process(cl_event, void *);
static void flush(void);

//******************************************************************************
// local data
//******************************************************************************
uint64_t pending_opencl_ops = 0;

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
  cl_generic_callback* cb_data = (cl_generic_callback*)user_data;
  uint64_t correlation_id = cb_data->correlation_id;
  opencl_call type = cb_data->type;

  if (event_command_exec_status == complete_flag) {
	gpu_correlation_id_map_entry_t *cid_map_entry = gpu_correlation_id_map_lookup(correlation_id);
	if (cid_map_entry == NULL) {
	  ETMSG(CL, "completion callback was called before registration callback. type: %d, correlation: %"PRIu64 "", type, correlation_id);
	}
	ETMSG(CL, "completion type: %d, correlation: %"PRIu64 "", type, correlation_id);
 	opencl_activity_completion_notify();
	opencl_activity_process(event, user_data);
  }
  free(event);
  free(user_data);
  opencl_pending_operations_adjust(-1);
}

void
opencl_finalize
(
  void
)
{
  flush();
  gpu_application_thread_process_activities();
}

//******************************************************************************
// private operations
//******************************************************************************
static void
opencl_pending_operations_adjust
(
  int value
)
{
  __atomic_fetch_add(&pending_opencl_ops, value, __ATOMIC_RELAXED);
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
flush
(
  void
)
{
  ETMSG(CL, "pending operations: %" PRIu64 "", __atomic_load_n(&pending_opencl_ops, __ATOMIC_SEQ_CST));
  while (__atomic_load_n(&pending_opencl_ops, __ATOMIC_SEQ_CST) != 0);
}
