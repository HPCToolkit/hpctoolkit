#include <gotcha/gotcha.h>
#include <stdio.h>
#include <CL/cl.h>
#include "opencl-api.h"
#include "opencl-intercept.h"

#define MALLOC 1
// function declarations
typedef cl_command_queue (*clqueue_fptr)(cl_context, cl_device_id, cl_command_queue_properties, cl_int *);
typedef cl_int (*clkernel_fptr)(cl_command_queue, cl_kernel, cl_uint, const size_t*, const size_t*, const size_t*, cl_uint, const cl_event*, cl_event*);
typedef cl_int (*clreadbuffer_fptr)(cl_command_queue, cl_mem, cl_bool, size_t, size_t, void *, cl_uint, const cl_event *, cl_event *);
typedef cl_int (*clwritebuffer_fptr)(cl_command_queue, cl_mem, cl_bool, size_t, size_t, const void *, cl_uint, const cl_event *, cl_event *);
typedef cl_int (*clSetKernelArg_fptr)(cl_kernel, cl_uint, size_t, const void *);

cl_event* eventNullCheck(cl_event*);
void event_callback(cl_event, cl_int, void *);
static cl_command_queue clCreateCommandQueue_wrapper(cl_context, cl_device_id, cl_command_queue_properties, cl_int *);
static cl_int clEnqueueReadBuffer_wrapper(cl_command_queue, cl_mem, cl_bool, size_t, size_t, void *, cl_uint, const cl_event *, cl_event *);
static cl_int clEnqueueWriteBuffer_wrapper(cl_command_queue, cl_mem, cl_bool, size_t, size_t, const void *, cl_uint, const cl_event *, cl_event *);
//static cl_int clSetKernelArg_wrapper(cl_kernel, cl_uint, size_t, const void *);
//static void mem_intercept(cl_kernel, size_t, cl_event *);
static cl_int clEnqueueNDRangeKernel_wrapper(cl_command_queue, cl_kernel, cl_uint, const size_t*, const size_t*, const size_t*,																		   cl_uint, const cl_event*, cl_event*);
profilingData* getEventTimeProfileInfo(cl_event);
profilingData* getMemoryProfileInfo(profilingData*, cl_memory_callback*);

// global variables
static gotcha_wrappee_handle_t clCreateCommandQueue_handle;
static gotcha_wrappee_handle_t clEnqueueNDRangeKernel_handle;
static gotcha_wrappee_handle_t clEnqueueReadBuffer_handle;
static gotcha_wrappee_handle_t clEnqueueWriteBuffer_handle;
//static gotcha_wrappee_handle_t clSetKernelArg_handle;

static cl_command_queue clCreateCommandQueue_wrapper(cl_context context, cl_device_id device, cl_command_queue_properties properties, cl_int *errcode_ret)
{
	properties |= (cl_command_queue_properties)CL_QUEUE_PROFILING_ENABLE; // enabling profiling
	clqueue_fptr clCreateCommandQueue_wrappee = (clqueue_fptr) gotcha_get_wrappee(clCreateCommandQueue_handle);
	return clCreateCommandQueue_wrappee(context, device, properties, errcode_ret);
}

static cl_int clEnqueueNDRangeKernel_wrapper(cl_command_queue command_queue, cl_kernel kernel, cl_uint work_dim, const size_t *global_work_offset, 													   const size_t *global_work_size, const size_t *local_work_size, cl_uint num_events_in_wait_list, 															 const cl_event *event_wait_list, cl_event *event)
{
	event = eventNullCheck(event);
    clkernel_fptr clEnqueueNDRangeKernel_wrappee = (clkernel_fptr) gotcha_get_wrappee(clEnqueueNDRangeKernel_handle);
	cl_int return_status = clEnqueueNDRangeKernel_wrappee(command_queue, kernel, work_dim, global_work_offset, global_work_size, local_work_size, num_events_in_wait_list, event_wait_list, event);
	if (MALLOC)
	{
		cl_kernel_callback *kernel_cb = (cl_kernel_callback*) malloc(sizeof(cl_kernel_callback));
		cct_node_t* node = createNode();
		kernel_cb->node = node;
		kernel_cb->type = "cl_kernel";
		printf("registering callback for type: %s\n", kernel_cb->type);
		clSetEventCallback(*event, CL_COMPLETE, &event_callback, kernel_cb);
	}
	else
	{
		cl_kernel_callback kernel_cb;
		memset(&kernel_cb, 0, sizeof(cl_kernel_callback));
		cct_node_t* node = createNode();
		kernel_cb.node = node;
		kernel_cb.type = "cl_kernel";
		printf("registering callback for type: %s\n", kernel_cb.type);
		clSetEventCallback(*event, CL_COMPLETE, &event_callback, &kernel_cb);
	}
	return return_status;
}

static cl_int clEnqueueReadBuffer_wrapper (cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_read, size_t offset, size_t cb,															 void *ptr, cl_uint num_events_in_wait_list, const cl_event *event_wait_list, cl_event *event)
{
	event = eventNullCheck(event);
    clreadbuffer_fptr clEnqueueReadBuffer_wrappee = (clreadbuffer_fptr) gotcha_get_wrappee(clEnqueueReadBuffer_handle);
	cl_int return_status = clEnqueueReadBuffer_wrappee(command_queue, buffer, blocking_read, offset, cb, ptr, num_events_in_wait_list, event_wait_list, event);
	printf("%zu(bytes) of data being transferred from device to host\n", cb); // pass this data
	if (MALLOC)	
	{
		cl_memory_callback *mem_transfer_cb = (cl_memory_callback*) malloc(sizeof(cl_memory_callback));
		cct_node_t* node = createNode();
		mem_transfer_cb->node = node;
		mem_transfer_cb->type = "cl_mem_transfer";
		mem_transfer_cb->size = cb;
		mem_transfer_cb->fromDeviceToHost = true;
		printf("registering callback for type: %s\n", mem_transfer_cb->type);
		clSetEventCallback(*event, CL_COMPLETE, &event_callback, mem_transfer_cb);
	}
	else
	{
		cl_memory_callback mem_transfer_cb;
		memset(&mem_transfer_cb, 0, sizeof(cl_memory_callback));
		cct_node_t* node = createNode();
		mem_transfer_cb.node = node;
		mem_transfer_cb.type = "cl_mem_transfer";
		mem_transfer_cb.size = cb;
		mem_transfer_cb.fromDeviceToHost = true;
		printf("registering callback for type: %s\n", mem_transfer_cb.type);
		clSetEventCallback(*event, CL_COMPLETE, &event_callback, &mem_transfer_cb);
	}
	return return_status;
}

static cl_int clEnqueueWriteBuffer_wrapper(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_write, size_t offset, size_t cb,														  const void *ptr, cl_uint num_events_in_wait_list, const cl_event *event_wait_list, cl_event *event)
{
	event = eventNullCheck(event);
    clwritebuffer_fptr clEnqueueWriteBuffer_wrappee = (clwritebuffer_fptr) gotcha_get_wrappee(clEnqueueWriteBuffer_handle);
	cl_int return_status = clEnqueueWriteBuffer_wrappee(command_queue, buffer, blocking_write, offset, cb, ptr, num_events_in_wait_list, event_wait_list, event);
	printf("%zu(bytes) of data being transferred from host to device\n", cb); // pass this data
	if (MALLOC)
	{
		cl_memory_callback *mem_transfer_cb = (cl_memory_callback*) malloc(sizeof(cl_memory_callback));
		cct_node_t* node = createNode();
		mem_transfer_cb->node = node;
		mem_transfer_cb->type = "cl_mem_transfer";
		mem_transfer_cb->size = cb;
		mem_transfer_cb->fromHostToDevice = true;
		printf("registering callback for type: %s\n", mem_transfer_cb->type);
		clSetEventCallback(*event, CL_COMPLETE, &event_callback, mem_transfer_cb);
	}
	else
	{
		cl_memory_callback mem_transfer_cb;
		memset(&mem_transfer_cb, 0, sizeof(cl_memory_callback));
		cct_node_t* node = createNode();
		mem_transfer_cb.node = node;
		mem_transfer_cb.type = "cl_mem_transfer";
		mem_transfer_cb.size = cb;
		mem_transfer_cb.fromHostToDevice = true;
		printf("registering callback for type: %s\n", mem_transfer_cb.type);
		clSetEventCallback(*event, CL_COMPLETE, &event_callback, &mem_transfer_cb);
	}
	return return_status;
}

cl_event* eventNullCheck(cl_event* event)
{
	if(!event)
	{
    	cl_event *new_event = (cl_event*)malloc(sizeof(cl_event));
		//memset(&new_event, 0, sizeof(cl_event));
		return new_event;
	}
	return event;
}

void event_callback(cl_event event, cl_int event_command_exec_status, void *user_data)
{
	cl_generic_callback* cb_data = (cl_generic_callback*)user_data;
	printf("inside callback function. The callback type is: %s\n", cb_data->type);
	profilingData* pd;
	if (strcmp(cb_data->type, "cl_kernel") == 0)
	{
		printf("Saving kernel data to node\n");
		pd = getEventTimeProfileInfo(event);
		updateNodeWithKernelProfileData(cb_data->node, pd);
	}
	else if (strcmp(cb_data->type, "cl_mem_transfer") == 0)
	{
		printf("Saving mem data to node\n");
		pd = getEventTimeProfileInfo(event);
		pd = getMemoryProfileInfo(pd, (cl_memory_callback*)user_data);
		updateNodeWithMemTransferProfileData(cb_data->node, pd);
	}
}

profilingData* getEventTimeProfileInfo(cl_event event)
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
	//memset(&pd, 0, sizeof(profilingData)); //str.h
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
	return pd;
}

static gotcha_binding_t queue_wrapper[] = {{"clCreateCommandQueue", (void*) clCreateCommandQueue_wrapper, &clCreateCommandQueue_handle}};
static gotcha_binding_t kernel_wrapper[] = {{"clEnqueueNDRangeKernel", (void*)clEnqueueNDRangeKernel_wrapper, &clEnqueueNDRangeKernel_handle}};
static gotcha_binding_t buffer_wrapper[] = {{"clEnqueueReadBuffer", (void*) clEnqueueReadBuffer_wrapper, &clEnqueueReadBuffer_handle},
											{"clEnqueueWriteBuffer", (void*) clEnqueueWriteBuffer_wrapper, &clEnqueueWriteBuffer_handle}};
//{"clSetKernelArg", (void*) clSetKernelArg_wrapper, &clSetKernelArg_handle}

void setup_opencl_intercept()
{   
	gotcha_wrap(queue_wrapper, 1, "queue_intercept");
	gotcha_wrap(kernel_wrapper, 1, "kernel_intercept");
	gotcha_wrap(buffer_wrapper, 2, "memory_intercept");
}

void teardown_opencl_intercept()
{
	// not sure if this works
	gotcha_set_priority("queue_intercept", -1);
	gotcha_set_priority("kernel_intercept", -1);
	gotcha_set_priority("memory_intercept", -1);
}

/*
gcc gotcha-opencl-wrapper.c -lOpenCL -shared -lgotcha -fPIC -o gotcha-opencl-wrapper.so
LD_PRELOAD=./gotcha-opencl-wrapper.so ./hello
*/
