#include <hpcrun/cct/cct.h> // cct_node_t
#include <stdbool.h> 

struct cl_generic_callback {char* type; cct_node_t* node;};
struct cl_kernel_callback {char* type; cct_node_t* node;};
struct cl_memory_callback {char* type; bool fromHostToDevice; bool fromDeviceToHost; size_t size; cct_node_t* node;};

void setup_opencl_intercept();

void teardown_opencl_intercept();
