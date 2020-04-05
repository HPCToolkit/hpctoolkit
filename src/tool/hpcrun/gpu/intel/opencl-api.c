#include <hpcrun/sample_event.h> // hpcrun_sample_callpath()
#include <hpcrun/safe-sampling.h> // enter() and exit()
#include <ucontext.h> //ucontext , but not imported in gpu-application-thread-api.c
#include <lib/prof-lean/hpcrun-fmt.h> // hpcrun_metricVal_t
#include <hpcrun/gpu/gpu-activity.h> // gpu_activity_t, GPU_ACTIVITY_KERNEL, gpu_memcpy_t, gpu_kernel_t, gpu_memcpy_type_t
#include <hpcrun/gpu/gpu-activity-process.h> //gpu_activity_process
#include <string.h> //memset
#include "opencl-api.h"
#include <hpcrun/gpu/gpu-correlation-channel.h> //gpu_correlation_channel_produce
#include <hpcrun/gpu/gpu-correlation-id-map.h> //gpu_correlation_id_map_insert
#include <hpcrun/gpu/gpu-host-correlation-map.h> //gpu_host_correlation_map_insert
#include <hpcrun/gpu/gpu-application-thread-api.h> // gpu_application_thread_correlation_callback
#include <hpcrun/gpu/gpu-op-placeholders.h> // gpu_op_placeholder_flags_set, gpu_placeholder_type_copy, gpu_op_placeholder_ip, gpu_op_ccts_insert
#include <hpcrun/gpu/gpu-metrics.h> //gpu_metrics_attribute
#include <hpcrun/gpu/gpu-event-id-map.h> //gpu_event_id_map_insert
#include "opencl-activity-translate.h"
#include <lib/prof-lean/usec_time.h> //usec_time
#include <hpcrun/gpu/gpu-monitoring-thread-api.h>
#include <hpcrun/gpu/gpu-activity-channel.h> //gpu_activity_channel_consume
#include <inttypes.h> //PRIu64
#define CPU_NANOTIME() (usec_time() * 1000)

void opencl_buffer_completion_notify();
void opencl_activity_process(cl_event, void *);

void opencl_function_submitted(opencl_call type, uint64_t correlation_id)
{
  gpu_correlation_id_map_insert(correlation_id, correlation_id);
  cct_node_t *api_node = gpu_application_thread_correlation_callback(correlation_id);
  gpu_op_placeholder_flags_t gpu_op_placeholder_flags = 0;
  gpu_op_ccts_t gpu_op_ccts;

  hpcrun_safe_enter();
  switch (type)
  {
	case memcpy_H2D:
	  gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags, gpu_placeholder_type_copyin);
	  gpu_op_ccts_insert(api_node, &gpu_op_ccts, gpu_op_placeholder_flags);
	  break;
	case memcpy_D2H:
	  gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags, gpu_placeholder_type_copyout);
	  gpu_op_ccts_insert(api_node, &gpu_op_ccts, gpu_op_placeholder_flags);
	  break;
	case kernel:
	  gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags, gpu_placeholder_type_kernel);
	  gpu_op_ccts_insert(api_node, &gpu_op_ccts, gpu_op_placeholder_flags);
	  break;
  }
  hpcrun_safe_exit();

  gpu_activity_channel_consume(gpu_metrics_attribute);	
  uint64_t cpu_submit_time = CPU_NANOTIME();
  gpu_correlation_channel_produce(correlation_id, &gpu_op_ccts, cpu_submit_time);
}

void opencl_subscriber_callback(cl_event event, cl_int event_command_exec_status, void *user_data)
{
  cl_int submit_flag = CL_SUBMITTED;
  if (event_command_exec_status == submit_flag)
  {
	cl_generic_callback* cb_data = (cl_generic_callback*)user_data;
	printf("submitting type: %d, correlation: %"PRIu64 "\n", cb_data->type, cb_data->correlation_id);
	opencl_function_submitted(cb_data->type, cb_data->correlation_id);
  }
}

void opencl_buffer_completion_callback(cl_event event, cl_int event_command_exec_status, void *user_data)
{
  cl_int complete_flag = CL_COMPLETE;
  cl_generic_callback* cb_data = (cl_generic_callback*)user_data;
  uint64_t correlation_id = cb_data->correlation_id;
  opencl_call type = cb_data->type;

  if (event_command_exec_status == complete_flag)
  {
	gpu_correlation_id_map_entry_t *cid_map_entry = gpu_correlation_id_map_lookup(correlation_id);
	if (cid_map_entry == NULL)
	{
		printf("completion callback was called before registration callback. type: %d, correlation: %"PRIu64 "\n", type, correlation_id);
		//opencl_function_submitted(type, correlation_id);
	}
	printf("completion type: %d, correlation: %"PRIu64 "\n", type, correlation_id);
 	opencl_buffer_completion_notify();
	opencl_activity_process(event, user_data);
  }
}

void opencl_buffer_completion_notify()
{
  gpu_monitoring_thread_activities_ready();
}

void opencl_activity_process(cl_event event, void * user_data)
{
  gpu_activity_t gpu_activity;
  opencl_activity_translate(&gpu_activity, event, user_data);
  gpu_activity_process(&gpu_activity);
  //gpu_metrics_attribute(&gpu_activity);
}
