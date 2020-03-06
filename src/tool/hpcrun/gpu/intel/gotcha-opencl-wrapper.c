#include <gotcha/gotcha.h>
#include <stdio.h>
#include <CL/cl.h>

/* add this file's code to keren's library_map() code
	try to remember why static is being used*/ 
struct profilingData
{
	cl_ulong queueTime;
	cl_ulong submitTime;
	cl_ulong startTime;
	cl_ulong endTime;
};

struct cl_callback {char* type; size_t size;} kernel_cb;

// function declarations
typedef cl_command_queue (*clqueue_fptr)(cl_context, cl_device_id, cl_command_queue_properties, cl_int *);
typedef cl_int (*clkernel_fptr)(cl_command_queue, cl_kernel, cl_uint, const size_t*, const size_t*, const size_t*, cl_uint, const cl_event*, cl_event*);
typedef cl_int (*clreadbuffer_fptr)(cl_command_queue, cl_mem, cl_bool, size_t, size_t, void *, cl_uint, const cl_event *, cl_event *);
typedef cl_int (*clwritebuffer_fptr)(cl_command_queue, cl_mem, cl_bool, size_t, size_t, const void *, cl_uint, const cl_event *, cl_event *);

cl_event* eventNullCheck(cl_event*);
void event_callback(cl_event, cl_int, void *);
void test(cl_event);
struct profilingData* getKernelProfilingInfo(cl_event);
static cl_command_queue clCreateCommandQueue_wrapper(cl_context, cl_device_id, cl_command_queue_properties, cl_int *);
static cl_int clEnqueueReadBuffer_wrapper(cl_command_queue, cl_mem, cl_bool, size_t, size_t, void *, cl_uint, const cl_event *, cl_event *);
static cl_int clEnqueueWriteBuffer_wrapper(cl_command_queue, cl_mem, cl_bool, size_t, size_t, const void *, cl_uint, const cl_event *, cl_event *);
static cl_int clEnqueueNDRangeKernel_wrapper(cl_command_queue, cl_kernel, cl_uint, const size_t*, const size_t*, const size_t*,																		   cl_uint, const cl_event*, cl_event*);

// global variables
static gotcha_wrappee_handle_t clCreateCommandQueue_handle;
static gotcha_wrappee_handle_t clEnqueueNDRangeKernel_handle;
static gotcha_wrappee_handle_t clEnqueueReadBuffer_handle;
static gotcha_wrappee_handle_t clEnqueueWriteBuffer_handle;

static cl_command_queue clCreateCommandQueue_wrapper(cl_context context, cl_device_id device, cl_command_queue_properties properties, cl_int *errcode_ret)
{
	properties |= (cl_command_queue_properties)CL_QUEUE_PROFILING_ENABLE; // enabling profiling
	clqueue_fptr clCreateCommandQueue_wrappee = (clqueue_fptr) gotcha_get_wrappee(clCreateCommandQueue_handle);
	printf("Queue modified\n");
	return clCreateCommandQueue_wrappee(context, device, properties, errcode_ret);
}

static cl_int clEnqueueNDRangeKernel_wrapper(cl_command_queue command_queue, cl_kernel kernel, cl_uint work_dim, const size_t *global_work_offset, 													   const size_t *global_work_size, const size_t *local_work_size, cl_uint num_events_in_wait_list, 															 const cl_event *event_wait_list, cl_event *event)
{
	event = eventNullCheck(event);
    clkernel_fptr clEnqueueNDRangeKernel_wrappee = (clkernel_fptr) gotcha_get_wrappee(clEnqueueNDRangeKernel_handle);
	cl_int return_status = clEnqueueNDRangeKernel_wrappee(command_queue, kernel, work_dim, global_work_offset, global_work_size, local_work_size, num_events_in_wait_list, event_wait_list, event);
	
	// setup callback where kernel profiling info will be collected
	kernel_cb.type = "cl_kernel";
    //kernel_cb.size = NULL;
	printf("cb_type: %s\n", kernel_cb.type);
	cl_int callback_status = clSetEventCallback(*event, CL_COMPLETE, &event_callback, &kernel_cb);
	return return_status;
}

static cl_int clEnqueueReadBuffer_wrapper (cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_read, size_t offset, size_t cb,															 void *ptr, cl_uint num_events_in_wait_list, const cl_event *event_wait_list, cl_event *event)
{
	event = eventNullCheck(event);
    clreadbuffer_fptr clEnqueueReadBuffer_wrappee = (clreadbuffer_fptr) gotcha_get_wrappee(clEnqueueReadBuffer_handle);
	// cb variable contains the size of the data being read from device to host. 
	printf("%zu(bytes) of data being transferred from device to host\n", cb); // pass this data
	cl_int return_status = clEnqueueReadBuffer_wrappee(command_queue, buffer, blocking_read, offset, cb, ptr, num_events_in_wait_list, event_wait_list, event);
	//cl_int callback_status = clSetEventCallback(*event, CL_COMPLETE, &event_callback, NULL);
	return return_status;
}


static cl_int clEnqueueWriteBuffer_wrapper(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_write, size_t offset, size_t cb,														  const void *ptr, cl_uint num_events_in_wait_list, const cl_event *event_wait_list, cl_event *event)
{
	event = eventNullCheck(event);
    clwritebuffer_fptr clEnqueueWriteBuffer_wrappee = (clwritebuffer_fptr) gotcha_get_wrappee(clEnqueueWriteBuffer_handle);
	// cb variable contains the size of the data being read from host to device. 
	printf("%zu(bytes) of data being transferred from host to device\n", cb); // pass this data
	cl_int return_status = clEnqueueWriteBuffer_wrappee(command_queue, buffer, blocking_write, offset, cb, ptr, num_events_in_wait_list, event_wait_list, event);
	//cl_int callback_status = clSetEventCallback(*event, CL_COMPLETE, &event_callback, NULL);
	return return_status;
}

cl_event* eventNullCheck(cl_event* event)
{
    cl_event* new_event;
	if(!event)
		return new_event;
	return event;
}

void event_callback(cl_event event, cl_int event_command_exec_status, void *user_data)
{
	if (event)
		printf("check0\n");
	printf("%ld\n", (&event));
	printf("inside callback function. The command execution status for the event is : %d\n", event_command_exec_status);
	// extracting the profiling numbers
	struct cl_callback* temp =  (struct cl_callback*)user_data;
	printf("the callback type is: %s\n", temp->type);
	printf("check1\n");
	//struct profilingData* pd = getKernelProfilingInfo(event); // pass this data
	getKernelProfilingInfo(event); // pass this data
	printf("check4\n");
	if (event)
		printf("check0\n");
}

struct profilingData* getKernelProfilingInfo(cl_event event)
{
	printf("check00\n");
	cl_ulong commandQueued = 0;
	cl_ulong commandSubmit = 0;
	cl_ulong commandStart = 0;
	cl_ulong commandEnd = 0;
	cl_int errorCode = CL_SUCCESS;

	printf("check2\n");
	errorCode |= clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_QUEUED, sizeof(commandQueued), &commandQueued, NULL);
	errorCode |= clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_SUBMIT, sizeof(commandSubmit), &commandSubmit, NULL);
	errorCode |= clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START, sizeof(commandStart), &commandStart, NULL);
	errorCode |= clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END, sizeof(commandEnd), &commandEnd, NULL);
	printf("check3\n");
	if (errorCode != CL_SUCCESS)
	{
		printf("error in collecting profiling data.\n");
		return NULL;
	}
	else
	{
		struct profilingData* pd;
		pd->queueTime = commandQueued;
		pd->submitTime = commandSubmit;
		pd->startTime = commandStart;
		pd->endTime = commandEnd;
		return pd;
	}
}

static gotcha_binding_t queue_wrapper[] = {{"clCreateCommandQueue", (void*) clCreateCommandQueue_wrapper, &clCreateCommandQueue_handle}};
static gotcha_binding_t kernel_wrapper[] = {{"clEnqueueNDRangeKernel", (void*)clEnqueueNDRangeKernel_wrapper, &clEnqueueNDRangeKernel_handle}};
static gotcha_binding_t buffer_wrapper[] = {{"clEnqueueReadBuffer", (void*) clEnqueueReadBuffer_wrapper, &clEnqueueReadBuffer_handle},
											{"clEnqueueWriteBuffer", (void*) clEnqueueWriteBuffer_wrapper, &clEnqueueWriteBuffer_handle}};

__attribute__((constructor)) void construct()
{   
	gotcha_wrap(queue_wrapper, 1, "queue_intercept");
	gotcha_wrap(kernel_wrapper, 1, "kernel_intercept");
	gotcha_wrap(buffer_wrapper, 2, "buffer_intercept");
}

/*
gcc gotcha-opencl-wrapper.c -lOpenCL -shared -lgotcha -fPIC -o gotcha-opencl-wrapper.so
LD_PRELOAD=./gotcha-opencl-wrapper.so ./hello
*/
