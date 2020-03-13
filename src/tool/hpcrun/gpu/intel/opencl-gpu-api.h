#include <hpcrun/cct/cct.h> // cct_node_t
#include <hpcrun/sample_event.h> // hpcrun_sample_callpath()
#include <hpcrun/safe-sampling.h> // enter() and exit()
#include <ucontext.h> //ucontext , but not imported in gpu-application-thread-api.c
#include <lib/prof-lean/hpcrun-fmt.h> // hpcrun_metricVal_t
#include <hpcrun/gpu/gpu-activity.h> // gpu_activity_t, GPU_ACTIVITY_KERNEL
#include <hpcrun/gpu/gpu-metrics.h> // gpu_metrics_attribute

struct profilingData
{
	cl_ulong queueTime;
	cl_ulong submitTime;
	cl_ulong startTime;
	cl_ulong endTime;
};

struct gpu_interval_t* oclProfDataToGenericData(struct profilingData *);

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
	printf("Saving the data\n");
	//gpu_metrics_attribute_metric_time_interval(cct_node, METRIC_ID(GPU_TIME_KER), (gpu_interval_t *)pd);
	gpu_activity_t activity; // = (gpu_activity_t*)malloc(sizeof(gpu_activity_t));
	activity.cct_node = cct_node;
	activity.kind = GPU_ACTIVITY_KERNEL;
	// how to set KINFO to false so that we skip the if-loop
	activity.details.kernel = *(gpu_kernel_t*)oclProfDataToGenericData(pd);
	gpu_metrics_attribute(&activity);
}

struct gpu_interval_t* oclProfDataToGenericData(struct profilingData *pd)
{
	gpu_interval_t* generic_interval = (gpu_interval_t*)malloc(sizeof(gpu_interval_t));
	generic_interval->start = pd->startTime; // ulong to uint64? should we cast?
	generic_interval->end = pd->endTime;
	return generic_interval;
}
