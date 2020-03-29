#include <hpcrun/cct/cct.h> // cct_node_t
#include <stdbool.h> 

#ifndef _OPENCL_INTERCEPT_H_
#define _OPENCL_INTERCEPT_H_
typedef enum opencl_call {memcpy_H2D, memcpy_D2H, kernel} opencl_call;
typedef struct cl_generic_callback {opencl_call type; cct_node_t* node;} cl_generic_callback;
typedef struct cl_kernel_callback {opencl_call type; cct_node_t* node;} cl_kernel_callback;
typedef struct cl_memory_callback {opencl_call type; bool fromHostToDevice; bool fromDeviceToHost; size_t size; cct_node_t* node;} cl_memory_callback;
#endif

void setup_opencl_intercept();

void teardown_opencl_intercept();
