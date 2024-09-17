// SPDX-FileCopyrightText: 2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#include <CL/cl.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char kernelSource[] =
    "// OpenCL kernel. Each work item takes care of one element of c\n"
    "__kernel void vecAdd(  __global double *a,\n"
    "                       __global double *b,\n"
    "                       __global double *c,\n"
    "                       const unsigned int n)\n"
    "{\n"
    "    //Get our global thread ID\n"
    "    int id = get_global_id(0);\n"
    "\n"
    "    //Make sure we do not go out of bounds\n"
    "    if (id < n)\n"
    "        c[id] = a[id] + b[id];\n"
    "}\n";

static const char* kernelSources[] = {kernelSource};

static void handleCLError(cl_int error, int exitcode);

int main() {
  // Length of vectors
  unsigned int n = 100000;

  // Host input vectors
  double* h_a;
  double* h_b;
  // Host output vector
  double* h_c;

  // Device input buffers
  cl_mem d_a;
  cl_mem d_b;
  // Device output buffer
  cl_mem d_c;

  cl_device_id device_id; // device ID
  cl_context context;     // context
  cl_command_queue queue; // command queue
  cl_program program;     // program
  cl_kernel kernel;       // kernel

  // Size, in bytes, of each vector
  size_t bytes = n * sizeof(double);

  // Allocate memory for each vector on host
  h_a = (double*)malloc(bytes);
  h_b = (double*)malloc(bytes);
  h_c = (double*)malloc(bytes);

  // Initialize vectors on host
  for (int i = 0; i < n; ++i) {
    h_a[i] = sinf(i) * sinf(i);
    h_b[i] = cosf(i) * cosf(i);
  }

  size_t globalSize, localSize;
  cl_int err;

  // Number of work items in each local work group
  localSize = 64;

  // Number of total work items - localSize must be devisor
  globalSize = ceil(n / (float)localSize) * localSize;

  // Find a suitable platform and device, one that has an actual GPU attached
  {
    cl_uint num_plats;
    err = clGetPlatformIDs(0, NULL, &num_plats);
    if (err != CL_SUCCESS)
      handleCLError(err, 77); // OpenCL not actually available

    cl_platform_id* plats = malloc(num_plats * sizeof(cl_platform_id));
    err = clGetPlatformIDs(num_plats, plats, NULL);
    if (err != CL_SUCCESS)
      handleCLError(err, 1);

    bool found = false;
    for (cl_uint i = 0; i < num_plats; ++i) {
      {
        char name[4096] = {0};
        err = clGetPlatformInfo(plats[i], CL_PLATFORM_NAME, sizeof name, name, NULL);
        if (err != CL_SUCCESS)
          handleCLError(err, 1);
        fprintf(stderr, "Platform: %s\n", name);
      }

      err = clGetDeviceIDs(plats[i], CL_DEVICE_TYPE_GPU, 1, &device_id, NULL);
      if (err == CL_SUCCESS) {
        {
          char name[4096] = {0};
          err = clGetDeviceInfo(device_id, CL_DEVICE_NAME, sizeof name, name, NULL);
          if (err != CL_SUCCESS)
            handleCLError(err, 1);
          fprintf(stderr, "Device: %s\n", name);
        }

        found = true;
        break;
      }
    }
    free(plats);
    if (!found) {
      fprintf(stderr, "Unable to find an actual GPU to run on, aborting!\n");
      exit(77);
    }
  }

  // Create a context
  context = clCreateContext(0, 1, &device_id, NULL, NULL, &err);
  if (context == NULL)
    handleCLError(err, 1);

  // Create a command queue
  queue = clCreateCommandQueueWithProperties(context, device_id, NULL, &err);
  if (queue == NULL)
    handleCLError(err, 1);

  // Create the compute program from the source buffer
  program = clCreateProgramWithSource(context, 1, kernelSources, NULL, &err);
  if (program == NULL)
    handleCLError(err, 1);

  // Build the program executable
  err = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
  if (err != CL_SUCCESS) {
    char log[4096] = {0};
    size_t logsz;
    cl_uint xerr = clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG,
                                         sizeof log, log, &logsz);
    if (xerr != CL_SUCCESS) {
      fprintf(stderr, "OpenCL error, failed to get build log\n");
      handleCLError(err, 1);
    }
    fprintf(stderr, "OpenCL build log (%zu bytes): %s\n", logsz, log);
    handleCLError(err, 1);
  }

  // Create the compute kernel in the program we wish to run
  kernel = clCreateKernel(program, "vecAdd", &err);
  if (kernel == NULL)
    handleCLError(err, 1);

  // Create the input and output arrays in device memory for our calculation
  d_a = clCreateBuffer(context, CL_MEM_READ_ONLY, bytes, NULL, &err);
  if (d_a == NULL)
    handleCLError(err, 1);
  d_b = clCreateBuffer(context, CL_MEM_READ_ONLY, bytes, NULL, &err);
  if (d_b == NULL)
    handleCLError(err, 1);
  d_c = clCreateBuffer(context, CL_MEM_WRITE_ONLY, bytes, NULL, &err);
  if (d_c == NULL)
    handleCLError(err, 1);

  // Write our data set into the input array in device memory
  err = clEnqueueWriteBuffer(queue, d_a, CL_TRUE, 0, bytes, h_a, 0, NULL, NULL);
  if (err != CL_SUCCESS)
    handleCLError(err, 1);
  err = clEnqueueWriteBuffer(queue, d_b, CL_TRUE, 0, bytes, h_b, 0, NULL, NULL);
  if (err != CL_SUCCESS)
    handleCLError(err, 1);

  // Set the arguments to our compute kernel
  err = clSetKernelArg(kernel, 0, sizeof(cl_mem), &d_a);
  if (err != CL_SUCCESS)
    handleCLError(err, 1);
  err = clSetKernelArg(kernel, 1, sizeof(cl_mem), &d_b);
  if (err != CL_SUCCESS)
    handleCLError(err, 1);
  err = clSetKernelArg(kernel, 2, sizeof(cl_mem), &d_c);
  if (err != CL_SUCCESS)
    handleCLError(err, 1);
  err = clSetKernelArg(kernel, 3, sizeof(unsigned int), &n);
  if (err != CL_SUCCESS)
    handleCLError(err, 1);

  // Execute the kernel over the entire range of the data set
  err = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &globalSize, &localSize, 0, NULL,
                               NULL);
  if (err != CL_SUCCESS)
    handleCLError(err, 1);

  // Wait for the command queue to get serviced before reading back results
  err = clFinish(queue);
  if (err != CL_SUCCESS)
    handleCLError(err, 1);

  // Read the results from the device
  err = clEnqueueReadBuffer(queue, d_c, CL_TRUE, 0, bytes, h_c, 0, NULL, NULL);
  if (err != CL_SUCCESS)
    handleCLError(err, 1);

  // Sum up vector c and print result divided by n, this should equal 1 within error
  double sum = 0;
  for (int i = 0; i < n; ++i)
    sum += h_c[i];
  printf("final result: %f\n", sum / n);

  // release OpenCL resources
  clReleaseMemObject(d_a);
  clReleaseMemObject(d_b);
  clReleaseMemObject(d_c);
  clReleaseProgram(program);
  clReleaseKernel(kernel);
  clReleaseCommandQueue(queue);
  clReleaseContext(context);

  // release host memory
  free(h_a);
  free(h_b);
  free(h_c);

  return 0;
}

void handleCLError(cl_int error, int exitcode) {
  const char* errstr = NULL;
  switch (error) {
  // run-time and JIT compiler errors
  case CL_SUCCESS:
    errstr = "CL_SUCCESS";
    break;
  case CL_DEVICE_NOT_FOUND:
    errstr = "CL_DEVICE_NOT_FOUND";
    break;
  case CL_DEVICE_NOT_AVAILABLE:
    errstr = "CL_DEVICE_NOT_AVAILABLE";
    break;
  case CL_COMPILER_NOT_AVAILABLE:
    errstr = "CL_COMPILER_NOT_AVAILABLE";
    break;
  case CL_MEM_OBJECT_ALLOCATION_FAILURE:
    errstr = "CL_MEM_OBJECT_ALLOCATION_FAILURE";
    break;
  case CL_OUT_OF_RESOURCES:
    errstr = "CL_OUT_OF_RESOURCES";
    break;
  case CL_OUT_OF_HOST_MEMORY:
    errstr = "CL_OUT_OF_HOST_MEMORY";
    break;
  case CL_PROFILING_INFO_NOT_AVAILABLE:
    errstr = "CL_PROFILING_INFO_NOT_AVAILABLE";
    break;
  case CL_MEM_COPY_OVERLAP:
    errstr = "CL_MEM_COPY_OVERLAP";
    break;
  case CL_IMAGE_FORMAT_MISMATCH:
    errstr = "CL_IMAGE_FORMAT_MISMATCH";
    break;
  case CL_IMAGE_FORMAT_NOT_SUPPORTED:
    errstr = "CL_IMAGE_FORMAT_NOT_SUPPORTED";
    break;
  case CL_BUILD_PROGRAM_FAILURE:
    errstr = "CL_BUILD_PROGRAM_FAILURE";
    break;
  case CL_MAP_FAILURE:
    errstr = "CL_MAP_FAILURE";
    break;
  case CL_MISALIGNED_SUB_BUFFER_OFFSET:
    errstr = "CL_MISALIGNED_SUB_BUFFER_OFFSET";
    break;
  case CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST:
    errstr = "CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST";
    break;
  case CL_COMPILE_PROGRAM_FAILURE:
    errstr = "CL_COMPILE_PROGRAM_FAILURE";
    break;
  case CL_LINKER_NOT_AVAILABLE:
    errstr = "CL_LINKER_NOT_AVAILABLE";
    break;
  case CL_LINK_PROGRAM_FAILURE:
    errstr = "CL_LINK_PROGRAM_FAILURE";
    break;
  case CL_DEVICE_PARTITION_FAILED:
    errstr = "CL_DEVICE_PARTITION_FAILED";
    break;
  case CL_KERNEL_ARG_INFO_NOT_AVAILABLE:
    errstr = "CL_KERNEL_ARG_INFO_NOT_AVAILABLE";
    break;

  // compile-time errors
  case CL_INVALID_VALUE:
    errstr = "CL_INVALID_VALUE";
    break;
  case CL_INVALID_DEVICE_TYPE:
    errstr = "CL_INVALID_DEVICE_TYPE";
    break;
  case CL_INVALID_PLATFORM:
    errstr = "CL_INVALID_PLATFORM";
    break;
  case CL_INVALID_DEVICE:
    errstr = "CL_INVALID_DEVICE";
    break;
  case CL_INVALID_CONTEXT:
    errstr = "CL_INVALID_CONTEXT";
    break;
  case CL_INVALID_QUEUE_PROPERTIES:
    errstr = "CL_INVALID_QUEUE_PROPERTIES";
    break;
  case CL_INVALID_COMMAND_QUEUE:
    errstr = "CL_INVALID_COMMAND_QUEUE";
    break;
  case CL_INVALID_HOST_PTR:
    errstr = "CL_INVALID_HOST_PTR";
    break;
  case CL_INVALID_MEM_OBJECT:
    errstr = "CL_INVALID_MEM_OBJECT";
    break;
  case CL_INVALID_IMAGE_FORMAT_DESCRIPTOR:
    errstr = "CL_INVALID_IMAGE_FORMAT_DESCRIPTOR";
    break;
  case CL_INVALID_IMAGE_SIZE:
    errstr = "CL_INVALID_IMAGE_SIZE";
    break;
  case CL_INVALID_SAMPLER:
    errstr = "CL_INVALID_SAMPLER";
    break;
  case CL_INVALID_BINARY:
    errstr = "CL_INVALID_BINARY";
    break;
  case CL_INVALID_BUILD_OPTIONS:
    errstr = "CL_INVALID_BUILD_OPTIONS";
    break;
  case CL_INVALID_PROGRAM:
    errstr = "CL_INVALID_PROGRAM";
    break;
  case CL_INVALID_PROGRAM_EXECUTABLE:
    errstr = "CL_INVALID_PROGRAM_EXECUTABLE";
    break;
  case CL_INVALID_KERNEL_NAME:
    errstr = "CL_INVALID_KERNEL_NAME";
    break;
  case CL_INVALID_KERNEL_DEFINITION:
    errstr = "CL_INVALID_KERNEL_DEFINITION";
    break;
  case CL_INVALID_KERNEL:
    errstr = "CL_INVALID_KERNEL";
    break;
  case CL_INVALID_ARG_INDEX:
    errstr = "CL_INVALID_ARG_INDEX";
    break;
  case CL_INVALID_ARG_VALUE:
    errstr = "CL_INVALID_ARG_VALUE";
    break;
  case CL_INVALID_ARG_SIZE:
    errstr = "CL_INVALID_ARG_SIZE";
    break;
  case CL_INVALID_KERNEL_ARGS:
    errstr = "CL_INVALID_KERNEL_ARGS";
    break;
  case CL_INVALID_WORK_DIMENSION:
    errstr = "CL_INVALID_WORK_DIMENSION";
    break;
  case CL_INVALID_WORK_GROUP_SIZE:
    errstr = "CL_INVALID_WORK_GROUP_SIZE";
    break;
  case CL_INVALID_WORK_ITEM_SIZE:
    errstr = "CL_INVALID_WORK_ITEM_SIZE";
    break;
  case CL_INVALID_GLOBAL_OFFSET:
    errstr = "CL_INVALID_GLOBAL_OFFSET";
    break;
  case CL_INVALID_EVENT_WAIT_LIST:
    errstr = "CL_INVALID_EVENT_WAIT_LIST";
    break;
  case CL_INVALID_EVENT:
    errstr = "CL_INVALID_EVENT";
    break;
  case CL_INVALID_OPERATION:
    errstr = "CL_INVALID_OPERATION";
    break;
  case CL_INVALID_GL_OBJECT:
    errstr = "CL_INVALID_GL_OBJECT";
    break;
  case CL_INVALID_BUFFER_SIZE:
    errstr = "CL_INVALID_BUFFER_SIZE";
    break;
  case CL_INVALID_MIP_LEVEL:
    errstr = "CL_INVALID_MIP_LEVEL";
    break;
  case CL_INVALID_GLOBAL_WORK_SIZE:
    errstr = "CL_INVALID_GLOBAL_WORK_SIZE";
    break;
  case CL_INVALID_PROPERTY:
    errstr = "CL_INVALID_PROPERTY";
    break;
  case CL_INVALID_IMAGE_DESCRIPTOR:
    errstr = "CL_INVALID_IMAGE_DESCRIPTOR";
    break;
  case CL_INVALID_COMPILER_OPTIONS:
    errstr = "CL_INVALID_COMPILER_OPTIONS";
    break;
  case CL_INVALID_LINKER_OPTIONS:
    errstr = "CL_INVALID_LINKER_OPTIONS";
    break;
  case CL_INVALID_DEVICE_PARTITION_COUNT:
    errstr = "CL_INVALID_DEVICE_PARTITION_COUNT";
    break;

  default:
    fprintf(stderr, "OpenCL ERROR: Unknown error %d\n", error);
    exit(exitcode);
  }

  fprintf(stderr, "OpenCL ERROR: %s\n", errstr);
  exit(exitcode);
}
