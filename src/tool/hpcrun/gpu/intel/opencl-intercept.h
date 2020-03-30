#include <hpcrun/cct/cct.h> // cct_node_t
#include <stdbool.h> 

#ifndef _OPENCL_INTERCEPT_H_
#define _OPENCL_INTERCEPT_H_
typedef enum opencl_call {memcpy_H2D, memcpy_D2H, kernel} opencl_call;
typedef struct cl_generic_callback {int correlation_id; opencl_call type; cct_node_t* cct_node;} cl_generic_callback;
typedef struct cl_kernel_callback {int correlation_id; opencl_call type; cct_node_t* cct_node;} cl_kernel_callback;
typedef struct cl_memory_callback {int correlation_id; opencl_call type; cct_node_t* cct_node; bool fromHostToDevice; bool fromDeviceToHost; size_t size; } cl_memory_callback;
#endif

void setup_opencl_intercept();
void teardown_opencl_intercept();
