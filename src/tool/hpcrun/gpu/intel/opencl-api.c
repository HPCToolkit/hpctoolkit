#include <hpcrun/sample_event.h> // hpcrun_sample_callpath()
#include <hpcrun/safe-sampling.h> // enter() and exit()
#include <ucontext.h> //ucontext , but not imported in gpu-application-thread-api.c
#include <lib/prof-lean/hpcrun-fmt.h> // hpcrun_metricVal_t
#include <hpcrun/gpu/gpu-activity.h> // gpu_activity_t, GPU_ACTIVITY_KERNEL, gpu_memcpy_t, gpu_kernel_t, gpu_memcpy_type_t
#include <hpcrun/gpu/gpu-activity-process.h>
#include <hpcrun/gpu/gpu-correlation-id-map.h>
#include <string.h> //memset
#include "opencl-api.h"
#include <hpcrun/gpu/gpu-application-thread-api.h> // gpu_application_thread_correlation_callback
#include <hpcrun/gpu/gpu-op-placeholders.h> // gpu_op_placeholder_flags_set, gpu_placeholder_type_copy, gpu_op_placeholder_ip, gpu_op_ccts_insert
#include <hpcrun/gpu/gpu-metrics.h> //gpu_metrics_attribute
#include "opencl-activity-translate.h"

void opencl_activity_process(cl_event, void *);

uint32_t correlation_id = 0;

cct_node_t * opencl_subscriber_callback(opencl_call type, uint32_t * correlation_id_arg)
{
	gpu_correlation_id_map_insert(correlation_id, correlation_id);
	*correlation_id_arg = correlation_id;
	cct_node_t *api_node = gpu_application_thread_correlation_callback(correlation_id);
	__atomic_fetch_add(&correlation_id, 1, __ATOMIC_SEQ_CST);
	gpu_op_placeholder_flags_t gpu_op_placeholder_flags = 0;
	gpu_op_ccts_t gpu_op_ccts;
	int cct_node_index = 0; // gpu_placeholder_type is an enum which will also be index in gpu_op_ccts[]	
	
	hpcrun_safe_enter();
    switch (type)
	{
		case memcpy_H2D:
			cct_node_index = gpu_placeholder_type_copyin;
			gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags, gpu_placeholder_type_copyin);
			gpu_op_ccts_insert(api_node, &gpu_op_ccts, gpu_op_placeholder_flags);
			break;
		case memcpy_D2H:
			cct_node_index = gpu_placeholder_type_copyout;
			gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags, gpu_placeholder_type_copyout);
			gpu_op_ccts_insert(api_node, &gpu_op_ccts, gpu_op_placeholder_flags);
			break;
		case kernel:
			cct_node_index = gpu_placeholder_type_kernel;
			gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags, gpu_placeholder_type_kernel);
			gpu_op_ccts_insert(api_node, &gpu_op_ccts, gpu_op_placeholder_flags);
			break;
	}
	hpcrun_safe_exit();

	cct_node_t *node = gpu_op_ccts.ccts[cct_node_index];
	return node;
}

void opencl_buffer_completion_callback(cl_event event, cl_int event_command_exec_status, void *user_data)
{
	opencl_activity_process(event, user_data);
}

void opencl_activity_process(cl_event event, void * user_data)
{
	gpu_activity_t gpu_activity;
	opencl_activity_translate(&gpu_activity, event, user_data);
	gpu_metrics_attribute(&gpu_activity);
}
