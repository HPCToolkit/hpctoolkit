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

struct gpu_kernel_t openclKernelDataToGenericKernelData(struct profilingData *);
struct gpu_memcpy_t openclMemDataToGenericMemData(struct profilingData *);
int zero_metric_id = 0; // nothing to see here
hpcrun_metricVal_t zero_metric_incr = {.i = 1};
uint32_t CORRELATION_ID = 0;
uint64_t host_correlation_id = 12;

cct_node_t* createNode()
{
	cct_node_t *api_node = gpu_application_thread_correlation_callback(host_correlation_id);
	/*ucontext_t uc;
	getcontext(&uc); // current context, where unwind will begin
	hpcrun_safe_enter();
	cct_node_t *node = hpcrun_sample_callpath(&uc, zero_metric_id, zero_metric_incr, 0, 1, NULL).sample_node;
	hpcrun_safe_exit();*/
	return api_node;
}

void register_external_correlation(gpu_activity_t activity)
{
	activity.kind = GPU_ACTIVITY_EXTERNAL_CORRELATION;
	activity.details.correlation.host_correlation_id = host_correlation_id;	
	gpu_activity_process(&activity);
}

void updateNodeWithKernelProfileData(cct_node_t* not_used__node, struct profilingData *pd)
{
	gpu_activity_t activity; // = (gpu_activity_t*)malloc(sizeof(gpu_activity_t));
	memset(&activity, 0, sizeof(gpu_activity_t));
	
	ucontext_t uc;
	getcontext(&uc); // current context, where unwind will begin
	cct_node_t *api_node = hpcrun_sample_callpath(&uc, zero_metric_id, zero_metric_incr, 0, 1, NULL).sample_node;	
	/* The same line works for memory operations, but not for kernel*/
	//cct_node_t *api_node = gpu_application_thread_correlation_callback(CORRELATION_ID++); 
	
	gpu_op_placeholder_flags_t gpu_op_placeholder_flags = 0;
	gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags, gpu_placeholder_type_kernel);
	
	gpu_op_ccts_t gpu_op_ccts;
	hpcrun_safe_enter();
    gpu_op_ccts_insert(api_node, &gpu_op_ccts, gpu_op_placeholder_flags);
	hpcrun_safe_exit();
	
	cct_node_t *node = gpu_op_ccts.ccts[5];	
	//cct_node_t *node = hpcrun_cct_insert_ip_norm(api_node, gpu_op_placeholder_ip(gpu_placeholder_type_copy)); 
	activity.cct_node = node;
	register_external_correlation(activity);
	
	activity.kind = GPU_ACTIVITY_KERNEL;
	activity.details.kernel = openclKernelDataToGenericKernelData(pd);
	uint32_t correlation_id = CORRELATION_ID++;
	activity.details.kernel.correlation_id = correlation_id;
	if (gpu_correlation_id_map_lookup(correlation_id) == NULL)
	{
		gpu_correlation_id_map_insert(correlation_id, correlation_id);
	}
	gpu_metrics_attribute(&activity); 
}

void updateNodeWithMemTransferProfileData(cct_node_t* not_used__node, struct profilingData *pd)
{
	gpu_activity_t activity;
	memset(&activity, 0, sizeof(gpu_activity_t));
	
	cct_node_t *api_node = gpu_application_thread_correlation_callback(CORRELATION_ID++);
	
	gpu_op_placeholder_flags_t gpu_op_placeholder_flags = 0;
	gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags, gpu_placeholder_type_copy);
	
	gpu_op_ccts_t gpu_op_ccts;
	hpcrun_safe_enter();
    gpu_op_ccts_insert(api_node, &gpu_op_ccts, gpu_op_placeholder_flags);
	hpcrun_safe_exit();
	
	cct_node_t *node = gpu_op_ccts.ccts[0];	
	//cct_node_t *node = hpcrun_cct_insert_ip_norm(api_node, gpu_op_placeholder_ip(gpu_placeholder_type_copy)); 
	activity.cct_node = node;
	register_external_correlation(activity);
	activity.kind = GPU_ACTIVITY_MEMCPY;
	activity.details.memcpy = openclMemDataToGenericMemData(pd);
	uint32_t correlation_id = CORRELATION_ID++;
	activity.details.memcpy.correlation_id = correlation_id;
	if (gpu_correlation_id_map_lookup(correlation_id) == NULL)
	{
		gpu_correlation_id_map_insert(correlation_id, correlation_id);
	}
	gpu_metrics_attribute(&activity);
}

struct gpu_kernel_t openclKernelDataToGenericKernelData(struct profilingData *pd)
{
	gpu_kernel_t generic_data; // = (gpu_kernel_t*)malloc(sizeof(gpu_kernel_t));
	memset(&generic_data, 0, sizeof(gpu_kernel_t));
	generic_data.start = (uint64_t)pd->startTime;
	generic_data.end = (uint64_t)pd->endTime;
	return generic_data;
}

struct gpu_memcpy_t openclMemDataToGenericMemData(struct profilingData *pd)
{
	gpu_memcpy_t generic_data; // = (gpu_memcpy_t*)malloc(sizeof(gpu_memcpy_t));
	memset(&generic_data, 0, sizeof(gpu_memcpy_t));
	generic_data.start = (uint64_t)pd->startTime;
	generic_data.end = (uint64_t)pd->endTime;
	generic_data.bytes = (uint64_t)pd->size;
	generic_data.copyKind = (gpu_memcpy_type_t) (pd->fromHostToDevice)? GPU_MEMCPY_H2D: pd->fromDeviceToHost? GPU_MEMCPY_D2H:
												 															   GPU_MEMCPY_UNK;
	return generic_data;
}
