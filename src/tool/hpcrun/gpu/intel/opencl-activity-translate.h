#include <CL/cl.h>
#include <hpcrun/gpu/gpu-activity.h> // gpu_activity_t

void opencl_activity_translate(gpu_activity_t *, cl_event, void *user_data);
