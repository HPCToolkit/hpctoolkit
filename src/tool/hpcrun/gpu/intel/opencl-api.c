#include <hpcrun/sample_event.h> // hpcrun_sample_callpath()
#include <hpcrun/safe-sampling.h> // enter() and exit()
#include <ucontext.h> //ucontext , but not imported in gpu-application-thread-api.c
#include <lib/prof-lean/hpcrun-fmt.h> // hpcrun_metricVal_t
#include <hpcrun/gpu/gpu-activity.h> // gpu_activity_t, GPU_ACTIVITY_KERNEL, gpu_memcpy_t, gpu_kernel_t, gpu_memcpy_type_t
#include <hpcrun/gpu/gpu-metrics.h> // gpu_metrics_attribute
#include <string.h> //memset
#include "opencl-api.h"

struct gpu_kernel_t openclTimeDataToGenericTimeData(struct profilingData *);
struct gpu_memcpy_t openclMemDataToGenericMemData(struct profilingData *);

cct_node_t* createNode()
{
	//gpu_metrics_default_enable();
	ucontext_t uc;
	getcontext(&uc); // current context, where unwind will begin
	int zero_metric_id = 0; // nothing to see here
	hpcrun_metricVal_t zero_metric_incr = {.i = 0};
	hpcrun_safe_enter();
	cct_node_t *node = hpcrun_sample_callpath(&uc, zero_metric_id, zero_metric_incr, 0, 1, NULL).sample_node;
	hpcrun_safe_exit();
	return node;
}

void updateNodeWithTimeProfileData(cct_node_t* cct_node, struct profilingData *pd)
{
	gpu_activity_t activity; // = (gpu_activity_t*)malloc(sizeof(gpu_activity_t));
	memset(&activity, 0, sizeof(gpu_activity_t)); //str.h
	activity.cct_node = cct_node;
	activity.kind = GPU_ACTIVITY_KERNEL;
	activity.details.kernel = openclTimeDataToGenericTimeData(pd);
	gpu_metrics_attribute(&activity);
}

void updateNodeWithMemTransferProfileData(cct_node_t* cct_node, struct profilingData *pd)
{
	gpu_activity_t activity;
	memset(&activity, 0, sizeof(gpu_activity_t));
	activity.cct_node = cct_node;
	activity.kind = GPU_ACTIVITY_MEMCPY;
	activity.details.memcpy = openclMemDataToGenericMemData(pd);
	gpu_metrics_attribute(&activity);
}

struct gpu_kernel_t openclTimeDataToGenericTimeData(struct profilingData *pd)
{
	gpu_kernel_t* generic_data = (gpu_kernel_t*)malloc(sizeof(gpu_kernel_t));
	generic_data->start = (uint64_t)pd->startTime;
	generic_data->end = (uint64_t)pd->endTime;
	return *generic_data;
}

struct gpu_memcpy_t openclMemDataToGenericMemData(struct profilingData *pd)
{
	gpu_memcpy_t* generic_data = (gpu_memcpy_t*)malloc(sizeof(gpu_memcpy_t));
	generic_data->start = (uint64_t)pd->startTime;
	generic_data->end = (uint64_t)pd->endTime;
	generic_data->bytes = (uint64_t)pd->size;
	generic_data->copyKind = (gpu_memcpy_type_t) (pd->fromHostToDevice)? GPU_MEMCPY_H2D: pd->fromDeviceToHost? GPU_MEMCPY_D2H:
												 															   GPU_MEMCPY_UNK;
	return *generic_data;
}
