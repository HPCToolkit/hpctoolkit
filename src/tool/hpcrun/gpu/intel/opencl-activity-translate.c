#include "opencl-activity-translate.h"
#include "opencl-api.h" //profilingData

void convert_kernel_launch(gpu_activity_t *, void *, cl_event);
void convert_memcpy(gpu_activity_t *, void *, cl_event, gpu_memcpy_type_t);
profilingData* getTimingInfoFromClEvent(cl_event);
profilingData* getMemoryProfileInfo(profilingData*, cl_memory_callback*);
gpu_kernel_t openclKernelDataToGenericKernelData(struct profilingData *);
gpu_memcpy_t openclMemDataToGenericMemData(struct profilingData *);

void opencl_activity_translate(gpu_activity_t * ga, cl_event event, void * user_data)
{
	cl_generic_callback* cb_data = (cl_generic_callback*)user_data;
	opencl_call type = cb_data->type;
	switch (type)
	{
		case kernel:
			convert_kernel_launch(ga, user_data, event);
			break;
		case memcpy_H2D:
			convert_memcpy(ga, user_data, event, GPU_MEMCPY_H2D);
			break;
		case memcpy_D2H:
			convert_memcpy(ga, user_data, event, GPU_MEMCPY_D2H);
			break;
	}
}

void convert_kernel_launch(gpu_activity_t * ga, void * user_data, cl_event event)
{
	cl_kernel_callback* kernel_cb_data = (cl_kernel_callback*)user_data;
	printf("Saving kernel data to gpu_activity_t\n");
	profilingData* pd = getTimingInfoFromClEvent(event);
	ga->kind = GPU_ACTIVITY_KERNEL;
	ga->details.kernel = openclKernelDataToGenericKernelData(pd);
	ga->details.kernel.correlation_id = kernel_cb_data->correlation_id;
}

void convert_memcpy(gpu_activity_t * ga, void * user_data, cl_event event, gpu_memcpy_type_t kind)
{
	cl_memory_callback* memory_cb_data = (cl_memory_callback*)user_data;
	profilingData* pd = getTimingInfoFromClEvent(event);
	pd = getMemoryProfileInfo(pd, memory_cb_data);
	ga->kind = GPU_ACTIVITY_MEMCPY;
	ga->details.memcpy = openclMemDataToGenericMemData(pd);
	ga->details.memcpy.correlation_id = memory_cb_data->correlation_id;
	switch (kind)
	{
		case GPU_MEMCPY_H2D:
			printf("Saving memcpy_H2D data to gpu_activity_t\n");
			break;
		case GPU_MEMCPY_D2H:
			printf("Saving memcpy_D2H data to gpu_activity_t\n");
			break;
		default:
			printf("Unknown memcpy\n");
	}
}

profilingData* getTimingInfoFromClEvent(cl_event event)
{
	cl_ulong commandQueued = 0;
	cl_ulong commandSubmit = 0;
	cl_ulong commandStart = 0;
	cl_ulong commandEnd = 0;
	cl_int errorCode = CL_SUCCESS;

	errorCode |= clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_QUEUED, sizeof(commandQueued), &commandQueued, NULL);
	errorCode |= clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_SUBMIT, sizeof(commandSubmit), &commandSubmit, NULL);
	errorCode |= clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START, sizeof(commandStart), &commandStart, NULL);
	errorCode |= clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END, sizeof(commandEnd), &commandEnd, NULL);
	if (errorCode != CL_SUCCESS)
		printf("error in collecting profiling data.\n");	
	profilingData *pd = (profilingData*) malloc(sizeof(profilingData));
	pd->queueTime = commandQueued;
	pd->submitTime = commandSubmit;
	pd->startTime = commandStart;
	pd->endTime = commandEnd;
	return pd;
}

profilingData* getMemoryProfileInfo(profilingData* pd, cl_memory_callback* cb_data)
{
	pd->size = cb_data->size;
	pd->fromHostToDevice = cb_data->fromHostToDevice;
	pd->fromDeviceToHost = cb_data->fromDeviceToHost;
	return pd;
}

gpu_kernel_t openclKernelDataToGenericKernelData(profilingData *pd)
{
	gpu_kernel_t generic_data; // = (gpu_kernel_t*)malloc(sizeof(gpu_kernel_t));
	memset(&generic_data, 0, sizeof(gpu_kernel_t));
	generic_data.start = (uint64_t)pd->startTime;
	generic_data.end = (uint64_t)pd->endTime;
	return generic_data;
}

gpu_memcpy_t openclMemDataToGenericMemData(profilingData *pd)
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
