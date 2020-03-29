#include <CL/cl.h>
#include <hpcrun/cct/cct.h> // cct_node_t
#include <hpcrun/gpu/gpu-op-placeholders.h> // gpu_placeholder_type_t
#include "opencl-intercept.h"

#ifndef _OPENCL_API_H_
#define _OPENCL_API_H_
typedef struct profilingData
{
	cl_ulong queueTime;
	cl_ulong submitTime;
	cl_ulong startTime;
	cl_ulong endTime;
	size_t size;
	bool fromHostToDevice;
	bool fromDeviceToHost;
} profilingData;
#endif

cct_node_t* opencl_subscriber_callback(opencl_call, uint32_t *);
void opencl_buffer_completion_callback(cl_event, cl_int, void *);
