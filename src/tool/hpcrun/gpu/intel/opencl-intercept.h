#include <hpcrun/cct/cct.h> // cct_node_t
#include <stdbool.h> 

typedef struct cl_generic_callback {char* type; cct_node_t* node;} cl_generic_callback;
typedef struct cl_kernel_callback {char* type; cct_node_t* node;} cl_kernel_callback;
typedef struct cl_memory_callback {char* type; bool fromHostToDevice; bool fromDeviceToHost; size_t size; cct_node_t* node;} cl_memory_callback;

void setup_opencl_intercept();

void teardown_opencl_intercept();
