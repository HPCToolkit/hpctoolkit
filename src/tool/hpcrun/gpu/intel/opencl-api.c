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
#include <lib/prof-lean/usec_time.h>
#include <hpcrun/gpu/gpu-monitoring-thread-api.h>

#define CPU_NANOTIME() (usec_time() * 1000)

void opencl_buffer_completion_notify();
void opencl_activity_process(cl_event, void *);

uint64_t correlation_id = 0;

void opencl_subscriber_callback(opencl_call type, uint64_t * correlation_id_arg)
{
	uint64_t local_c_id;
	__atomic_load(&correlation_id, &local_c_id, __ATOMIC_SEQ_CST); //TODO
	gpu_correlation_id_map_insert(local_c_id, local_c_id);
	*correlation_id_arg = local_c_id;
	cct_node_t *api_node = gpu_application_thread_correlation_callback(local_c_id);
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
	
	gpu_application_thread_process_activities();
	uint64_t cpu_submit_time = CPU_NANOTIME();
    gpu_correlation_channel_produce(correlation_id, &gpu_op_ccts, cpu_submit_time);
	__atomic_fetch_add(&correlation_id, 1, __ATOMIC_SEQ_CST);
}

void opencl_buffer_completion_callback(cl_event event, cl_int event_command_exec_status, void *user_data)
{
	opencl_buffer_completion_notify();
	opencl_activity_process(event, user_data);
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
