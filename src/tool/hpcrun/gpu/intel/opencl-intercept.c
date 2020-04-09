#include <gotcha/gotcha.h>
#include <stdio.h>
#include <CL/cl.h>

#include "opencl-api.h"
#include "opencl-intercept.h"

// function declarations
typedef cl_command_queue (*clqueue_fptr)(cl_context, cl_device_id, cl_command_queue_properties, cl_int *);
typedef cl_int (*clkernel_fptr)(cl_command_queue, cl_kernel, cl_uint, const size_t*, const size_t*, const size_t*, cl_uint, const cl_event*, cl_event*);
typedef cl_int (*clreadbuffer_fptr)(cl_command_queue, cl_mem, cl_bool, size_t, size_t, void *, cl_uint, const cl_event *, cl_event *);
typedef cl_int (*clwritebuffer_fptr)(cl_command_queue, cl_mem, cl_bool, size_t, size_t, const void *, cl_uint, const cl_event *, cl_event *);
typedef cl_int (*clSetKernelArg_fptr)(cl_kernel, cl_uint, size_t, const void *);

cl_event* eventNullCheck(cl_event*);
static cl_command_queue clCreateCommandQueue_wrapper(cl_context, cl_device_id, cl_command_queue_properties, cl_int *);
static cl_int clEnqueueReadBuffer_wrapper(cl_command_queue, cl_mem, cl_bool, size_t, size_t, void *, cl_uint, const cl_event *, cl_event *);
static cl_int clEnqueueWriteBuffer_wrapper(cl_command_queue, cl_mem, cl_bool, size_t, size_t, const void *, cl_uint, const cl_event *, cl_event *);
static cl_int clEnqueueNDRangeKernel_wrapper(cl_command_queue, cl_kernel, cl_uint, const size_t*, const size_t*, const size_t*,																		   cl_uint, const cl_event*, cl_event*);

// global variables
static gotcha_wrappee_handle_t clCreateCommandQueue_handle;
static gotcha_wrappee_handle_t clEnqueueNDRangeKernel_handle;
static gotcha_wrappee_handle_t clEnqueueReadBuffer_handle;
static gotcha_wrappee_handle_t clEnqueueWriteBuffer_handle;
uint64_t correlation_id = 0;

static cl_command_queue clCreateCommandQueue_wrapper(cl_context context, cl_device_id device, cl_command_queue_properties properties, cl_int *errcode_ret)
{
  properties |= (cl_command_queue_properties)CL_QUEUE_PROFILING_ENABLE; // enabling profiling
  clqueue_fptr clCreateCommandQueue_wrappee = (clqueue_fptr) gotcha_get_wrappee(clCreateCommandQueue_handle);
  return clCreateCommandQueue_wrappee(context, device, properties, errcode_ret);
}

static cl_int clEnqueueNDRangeKernel_wrapper(cl_command_queue command_queue, cl_kernel ocl_kernel, cl_uint work_dim, const size_t *global_work_offset, 													   const size_t *global_work_size, const size_t *local_work_size, cl_uint num_events_in_wait_list, 															 const cl_event *event_wait_list, cl_event *event)
{
  uint64_t local_correlation_id = __atomic_fetch_add(&correlation_id, 1, __ATOMIC_SEQ_CST);
  cl_kernel_callback *kernel_cb = (cl_kernel_callback*) malloc(sizeof(cl_kernel_callback));
  kernel_cb->correlation_id = local_correlation_id;
  kernel_cb->type = kernel; 
  printf("registering callback for type: kernel\n");
  
  event = eventNullCheck(event);
  clkernel_fptr clEnqueueNDRangeKernel_wrappee = (clkernel_fptr) gotcha_get_wrappee(clEnqueueNDRangeKernel_handle);
  cl_int return_status = clEnqueueNDRangeKernel_wrappee(command_queue, ocl_kernel, work_dim, global_work_offset, global_work_size, local_work_size, num_events_in_wait_list, event_wait_list, event);
  
  opencl_subscriber_callback(kernel_cb->type, kernel_cb->correlation_id);
  clSetEventCallback(*event, CL_COMPLETE, &opencl_buffer_completion_callback, kernel_cb);
  return return_status;
}

static cl_int clEnqueueReadBuffer_wrapper (cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_read, size_t offset, size_t cb,															 void *ptr, cl_uint num_events_in_wait_list, const cl_event *event_wait_list, cl_event *event)
{
  uint64_t local_correlation_id = __atomic_fetch_add(&correlation_id, 1, __ATOMIC_SEQ_CST);
  cl_memory_callback *mem_transfer_cb = (cl_memory_callback*) malloc(sizeof(cl_memory_callback));
  mem_transfer_cb->correlation_id = local_correlation_id;
  mem_transfer_cb->type = memcpy_D2H; 
  mem_transfer_cb->size = cb;
  mem_transfer_cb->fromDeviceToHost = true;
  mem_transfer_cb->fromHostToDevice = false;
  printf("registering callback for type: D2H\n");
  
  event = eventNullCheck(event);
  clreadbuffer_fptr clEnqueueReadBuffer_wrappee = (clreadbuffer_fptr) gotcha_get_wrappee(clEnqueueReadBuffer_handle);
  cl_int return_status = clEnqueueReadBuffer_wrappee(command_queue, buffer, blocking_read, offset, cb, ptr, num_events_in_wait_list, event_wait_list, event);
  printf("%zu(bytes) of data being transferred from device to host\n", cb);

  opencl_subscriber_callback(mem_transfer_cb->type, mem_transfer_cb->correlation_id);
  clSetEventCallback(*event, CL_COMPLETE, &opencl_buffer_completion_callback, mem_transfer_cb);
  return return_status;
}

static cl_int clEnqueueWriteBuffer_wrapper(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_write, size_t offset, size_t cb,														  const void *ptr, cl_uint num_events_in_wait_list, const cl_event *event_wait_list, cl_event *event)
{
  uint64_t local_correlation_id = __atomic_fetch_add(&correlation_id, 1, __ATOMIC_SEQ_CST);
  cl_memory_callback *mem_transfer_cb = (cl_memory_callback*) malloc(sizeof(cl_memory_callback));
  mem_transfer_cb->correlation_id = local_correlation_id;
  mem_transfer_cb->type = memcpy_H2D; 
  mem_transfer_cb->size = cb;
  mem_transfer_cb->fromHostToDevice = true;
  mem_transfer_cb->fromDeviceToHost = false;
  printf("registering callback for type: H2D\n");
  
  event = eventNullCheck(event);
  clwritebuffer_fptr clEnqueueWriteBuffer_wrappee = (clwritebuffer_fptr) gotcha_get_wrappee(clEnqueueWriteBuffer_handle);
  cl_int return_status = clEnqueueWriteBuffer_wrappee(command_queue, buffer, blocking_write, offset, cb, ptr, num_events_in_wait_list, event_wait_list, event);
  printf("%zu(bytes) of data being transferred from host to device\n", cb);
  
  opencl_subscriber_callback(mem_transfer_cb->type, mem_transfer_cb->correlation_id);
  clSetEventCallback(*event, CL_COMPLETE, &opencl_buffer_completion_callback, mem_transfer_cb);
  free(event);
  return return_status;
}

cl_event* eventNullCheck(cl_event* event)
{
  if(!event)
  {
	cl_event *new_event = (cl_event*)malloc(sizeof(cl_event));
	return new_event;
  }
  return event;
}


static gotcha_binding_t queue_wrapper[] = {{"clCreateCommandQueue", (void*) clCreateCommandQueue_wrapper, &clCreateCommandQueue_handle}};
static gotcha_binding_t kernel_wrapper[] = {{"clEnqueueNDRangeKernel", (void*)clEnqueueNDRangeKernel_wrapper, &clEnqueueNDRangeKernel_handle}};
static gotcha_binding_t buffer_wrapper[] = {{"clEnqueueReadBuffer", (void*) clEnqueueReadBuffer_wrapper, &clEnqueueReadBuffer_handle},
  {"clEnqueueWriteBuffer", (void*) clEnqueueWriteBuffer_wrapper, &clEnqueueWriteBuffer_handle}};

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
