#include "cuda-api.h"
#include <stdio.h>
#include <cuda.h>

#define CUDA_API_DEBUG 0

#if CUDA_API_DEBUG
#define PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define PRINT(...)
#endif

#define HPCRUN_CUDA_API_CALL(fn, args)                              \
{                                                                   \
    CUresult error_result = fn args;                                \
    if (error_result != CUDA_SUCCESS) {                             \
        PRINT("cuda api %s returned %d\n", #fn, (int)error_result); \
        exit(-1);                                                   \
    }                                                               \
}

//*******************************************************************************
// device query
//*******************************************************************************

int cuda_device_sm_blocks_query(int major, int minor)
{
  if (major == 7) {
    return 32;
  } else if (major == 6) {
    return 32;
  } else {
    // TODO(Keren): add more devices
    return 8;
  }
}


void
cuda_device_property_query(int device_id, cupti_device_property_t *property)
{
  HPCRUN_CUDA_API_CALL(cuDeviceGetAttribute,
    (&property->sm_count, CU_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT, device_id));
  HPCRUN_CUDA_API_CALL(cuDeviceGetAttribute,
    (&property->sm_clock_rate, CU_DEVICE_ATTRIBUTE_CLOCK_RATE, device_id));
  HPCRUN_CUDA_API_CALL(cuDeviceGetAttribute,
    (&property->sm_shared_memory, CU_DEVICE_ATTRIBUTE_MAX_SHARED_MEMORY_PER_MULTIPROCESSOR, device_id));
  HPCRUN_CUDA_API_CALL(cuDeviceGetAttribute,
    (&property->sm_registers, CU_DEVICE_ATTRIBUTE_MAX_REGISTERS_PER_MULTIPROCESSOR, device_id));
  HPCRUN_CUDA_API_CALL(cuDeviceGetAttribute,
    (&property->sm_threads, CU_DEVICE_ATTRIBUTE_MAX_THREADS_PER_MULTIPROCESSOR, device_id));
  HPCRUN_CUDA_API_CALL(cuDeviceGetAttribute,
    (&property->num_threads_per_warp, CU_DEVICE_ATTRIBUTE_WARP_SIZE, device_id));
  int major = 0, minor = 0;
  HPCRUN_CUDA_API_CALL(cuDeviceGetAttribute,
    (&major, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, device_id));
  HPCRUN_CUDA_API_CALL(cuDeviceGetAttribute,
    (&minor, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR, device_id));
  property->sm_blocks = cuda_device_sm_blocks_query(major, minor);
}
