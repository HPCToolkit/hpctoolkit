#include <hpcrun/main.h> // hpcrun_force_dlopen
#include <hpcrun/sample-sources/libdl.h> //CHK_DLOPEN, CHK_DLSYM
#include <hpcrun/gpu/gpu-metrics.h> // gpu_metrics_default_enable, gpu_metrics_attribute

#include "opencl-setup.h"
#include "opencl-intercept.h"
#include <CL/cl.h>

#define FORALL_OPENCL_ROUTINES(macro)	\
  macro(clCreateCommandQueue)   		\
  macro(clEnqueueNDRangeKernel)			\
  macro(clEnqueueWriteBuffer)  			\
  macro(clEnqueueReadBuffer)

#define OPENCL_FN_NAME(f) DYN_FN_NAME(f)

#define OPENCL_FN(fn, args) \
  static cl_int (*OPENCL_FN_NAME(fn)) args

OPENCL_FN
(
  clCreateCommandQueue, (cl_context context, cl_device_id device, cl_command_queue_properties properties, cl_int *errcode_ret)
);

OPENCL_FN
(
  clEnqueueNDRangeKernel, (cl_command_queue command_queue, cl_kernel ocl_kernel, cl_uint work_dim, const size_t *global_work_offset, 													   const size_t *global_work_size, const size_t *local_work_size, cl_uint num_events_in_wait_list, 															 const cl_event *event_wait_list, cl_event *event)
);

OPENCL_FN
(
  clEnqueueReadBuffer, (cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_read, size_t offset, size_t cb,															 void *ptr, cl_uint num_events_in_wait_list, const cl_event *event_wait_list, cl_event *event)
);

OPENCL_FN
(
  clEnqueueWriteBuffer, (cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_write, size_t offset, size_t cb,														  const void *ptr, cl_uint num_events_in_wait_list, const cl_event *event_wait_list, cl_event *event)
);

static const char * opencl_path();

void opencl_initialize()
{
  gpu_metrics_default_enable();
  printf("We are setting up opencl intercepts\n");
  setup_opencl_intercept();
}

int opencl_bind()
{
  // This is a workaround so that we do not hang when taking timer interrupts
  setenv("HSA_ENABLE_INTERRUPT", "0", 1);

  #ifndef HPCRUN_STATIC_LINK  // dynamic libraries only availabile in non-static case
    hpcrun_force_dlopen(true);
    CHK_DLOPEN(opencl, opencl_path(), RTLD_NOW | RTLD_GLOBAL);
    hpcrun_force_dlopen(false);

    #define OPENCL_BIND(fn) \
      CHK_DLSYM(opencl, fn);

    FORALL_OPENCL_ROUTINES(OPENCL_BIND)

    #undef OPENCL_BIND

	return 0;
  #else
    return -1;
  #endif // ! HPCRUN_STATIC_LINK  
}

static const char * opencl_path()
{
  const char *path = "libOpenCL.so";
  return path;
}

/*
void opencl_finalize()
{
  teardown_opencl_intercept();
}*/
