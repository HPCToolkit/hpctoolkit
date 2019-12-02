//
// Created by tx7 on 8/15/19.
//

#include <stdio.h>
#include <errno.h>     // errno
#include <fcntl.h>     // open
#include <limits.h>    // PATH_MAX
// #include <pthread.h>
#include <stdio.h>     // sprintf
#include <unistd.h>
#include <sys/stat.h>  // mkdir

#include <monitor.h>

#ifndef HPCRUN_STATIC_LINK
#include <dlfcn.h>
#undef _GNU_SOURCE
#define _GNU_SOURCE
#include <link.h>          // dl_iterate_phdr
#include <linux/limits.h>  // PATH_MAX
#include <string.h>        // strstr
#endif

#include "roctracer-api.h"
#include <hpcrun/sample-sources/amd.h>
#include <hpcrun/gpu/gpu-op-placeholders.h>

#include <lib/prof-lean/stdatomic.h>
#include <lib/prof-lean/spinlock.h>
#include <lib/prof-lean/stdatomic.h>

#include <hpcrun/cct2metrics.h>
#include <hpcrun/files.h>
#include <hpcrun/hpcrun_stats.h>
#include <hpcrun/module-ignore-map.h>
#include <hpcrun/main.h>
#include <hpcrun/safe-sampling.h>
#include <hpcrun/sample_event.h>
#include <hpcrun/sample-sources/libdl.h>
#include <hpcrun/cct/cct.h>

#include <roctracer_hip.h>

//static atomic_long roctracer_correlation_id = ATOMIC_VAR_INIT(1);

#define FORALL_ROCTRACER_ROUTINES(macro)			\
  macro(roctracer_open_pool)		\
  macro(roctracer_enable_callback)  \
  macro(roctracer_enable_activity)  \
  macro(roctracer_disable_callback) \
  macro(roctracer_disable_activity) \
  macro(roctracer_flush_activity)   \
  macro(roctracer_activity_push_external_correlation_id) \
  macro(roctracer_activity_pop_external_correlation_id)


#define ROCTRACER_FN_NAME(f) DYN_FN_NAME(f)

#define ROCTRACER_FN(fn, args) \
  static roctracer_status_t (*ROCTRACER_FN_NAME(fn)) args

#define HPCRUN_ROCTRACER_CALL(fn, args) \
{      \
  roctracer_status_t status = ROCTRACER_FN_NAME(fn) args;	\
  if (status != ROCTRACER_STATUS_SUCCESS) {		\
    /* use roctracer_error_string() */ \
  }						\
}

//----------------------------------------------------------
// roctracer function pointers for late binding
//----------------------------------------------------------

ROCTRACER_FN
(
 roctracer_open_pool,
 (
  const roctracer_properties_t*,
  roctracer_pool_t**
 )
);

ROCTRACER_FN
(
 roctracer_enable_callback,
 (
  activity_rtapi_callback_t,
  void*
 )
);

ROCTRACER_FN
(
 roctracer_enable_activity,
 (
  roctracer_pool_t*
 )
);

ROCTRACER_FN
(
 roctracer_disable_callback,
 (

 )
);

ROCTRACER_FN
(
 roctracer_disable_activity,
 (
       
 )
);

ROCTRACER_FN
(
 roctracer_flush_activity,
 (
  roctracer_pool_t*
 )
);

ROCTRACER_FN
(
 roctracer_flush_activity,
 (
  roctracer_pool_t*
 )
);

ROCTRACER_FN
(
 roctracer_activity_push_external_correlation_id,
 (
  activity_correlation_id_t
 )
);

ROCTRACER_FN
(
 roctracer_activity_pop_external_correlation_id,
 (
  activity_correlation_id_t*
 )
);

void
roctracer_correlation_id_push(uint64_t id)
{
  HPCRUN_ROCTRACER_CALL(roctracer_activity_push_external_correlation_id,
    (id));
}

void
roctracer_correlation_id_pop(uint64_t* id)
{
  HPCRUN_ROCTRACER_CALL(roctracer_activity_pop_external_correlation_id,
    (id));
}


/*
static void
roctracer_kernel_data_set
(
        const hip_api_data_t *data,
        entry_data_t *entry_data,
        uint32_t callback_id
)
{
    switch(callback_id)
    {
        case HIP_API_ID_hipModuleLaunchKernel:
            entry_data->kernel.blockSharedMemory = data->args.hipModuleLaunchKernel.sharedMemBytes;
            entry_data->kernel.blockThreads = data->args.hipModuleLaunchKernel.blockDimX * data->args.hipModuleLaunchKernel.blockDimY * data->args.hipModuleLaunchKernel.blockDimZ;
            break;
        case HIP_API_ID_hipLaunchCooperativeKernel:
            entry_data->kernel.blockSharedMemory = data->args.hipLaunchCooperativeKernel.sharedMemBytes;
            entry_data->kernel.blockThreads = data->args.hipLaunchCooperativeKernel.blockDimX.x * data->args.hipLaunchCooperativeKernel.blockDimX.y * data->args.hipLaunchCooperativeKernel.blockDimX.z;
            break;
        case HIP_API_ID_hipHccModuleLaunchKernel:
            entry_data->kernel.blockSharedMemory = data->args.hipHccModuleLaunchKernel.sharedMemBytes;
            entry_data->kernel.blockThreads = data->args.hipHccModuleLaunchKernel.globalWorkSizeX * data->args.hipHccModuleLaunchKernel.globalWorkSizeY * data->args.hipHccModuleLaunchKernel.globalWorkSizeZ
                    + data->args.hipHccModuleLaunchKernel.localWorkSizeX * data->args.hipHccModuleLaunchKernel.localWorkSizeY * data->args.hipHccModuleLaunchKernel.localWorkSizeZ;
            break;
    }
}
*/


void
roctracer_subscriber_callback
(
      uint32_t domain,
      uint32_t callback_id,
      const void* callback_data,
      void* arg
)
{
    /* Will need to deal with place holder in this function */
    gpu_record_init();
    gpu_op_placeholder_flags_t gpu_op_placeholder_flags = 0;
    bool is_valid_op = false;
    const hip_api_data_t* data = (const hip_api_data_t*)(callback_data);    
    
    switch (callback_id) {
        case HIP_API_ID_hipMemcpy:
        case HIP_API_ID_hipMemcpyToSymbolAsync:
        case HIP_API_ID_hipMemcpyFromSymbolAsync:
        case HIP_API_ID_hipMemcpyDtoD:
        case HIP_API_ID_hipMemcpy2DToArray:
        case HIP_API_ID_hipMemcpyAsync:
        case HIP_API_ID_hipMemcpyFromSymbol:
        case HIP_API_ID_hipMemcpy3D:
        case HIP_API_ID_hipMemcpyAtoH:
        case HIP_API_ID_hipMemcpyHtoD:
        case HIP_API_ID_hipMemcpyHtoA:
        case HIP_API_ID_hipMemcpy2D:
        case HIP_API_ID_hipMemcpyPeerAsync:
        case HIP_API_ID_hipMemcpyDtoH:
        case HIP_API_ID_hipMemcpyHtoDAsync:
        case HIP_API_ID_hipMemcpyFromArray:
        case HIP_API_ID_hipMemcpy2DAsync:
        case HIP_API_ID_hipMemcpyToArray:
        case HIP_API_ID_hipMemcpyToSymbol:
        case HIP_API_ID_hipMemcpyPeer:
        case HIP_API_ID_hipMemcpyDtoDAsync:
        case HIP_API_ID_hipMemcpyDtoHAsync:
        case HIP_API_ID_hipMemcpyParam2D:
        {
            gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags, gpu_placeholder_type_copy);
            is_valid_op = true;
            break;
        }
        
        case HIP_API_ID_hipMalloc:
        case HIP_API_ID_hipMallocPitch:
        case HIP_API_ID_hipMalloc3DArray:
        case HIP_API_ID_hipMallocArray:
        case HIP_API_ID_hipHostMalloc:
        case HIP_API_ID_hipMallocManaged:
        case HIP_API_ID_hipMalloc3D:
        case HIP_API_ID_hipExtMallocWithFlags:
        {
            gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags, gpu_placeholder_type_alloc);
            is_valid_op = true;
            break;
        }            
        case HIP_API_ID_hipMemset3DAsync:
        case HIP_API_ID_hipMemset2D:
        case HIP_API_ID_hipMemset2DAsync:
        case HIP_API_ID_hipMemset:
        case HIP_API_ID_hipMemsetD8:
        case HIP_API_ID_hipMemset3D:
        case HIP_API_ID_hipMemsetAsync:
        case HIP_API_ID_hipMemsetD32Async:
        {
            gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags, gpu_placeholder_type_memset);
            is_valid_op = true;
            break;                
        }
        
        case HIP_API_ID_hipFree:
        case HIP_API_ID_hipFreeArray:
        {
            gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags, gpu_placeholder_type_delete);
            is_valid_op = true;
            break;                                
        }
        case HIP_API_ID_hipModuleLaunchKernel:
        case HIP_API_ID_hipLaunchCooperativeKernel:
        case HIP_API_ID_hipHccModuleLaunchKernel:
        //case HIP_API_ID_hipExtModuleLaunchKernel:
        {
            gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags, gpu_placeholder_type_kernel);
            is_valid_op = true;
            break;                
        }                
        case HIP_API_ID_hipCtxSynchronize:
        case HIP_API_ID_hipStreamSynchronize:
        case HIP_API_ID_hipDeviceSynchronize:
        case HIP_API_ID_hipEventSynchronize:
        {
            gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags, gpu_placeholder_type_sync);
            is_valid_op = true;
            break;                

        }
        default:
            break;
    }

    if (!is_valid_op) return;


    if (data->phase == ACTIVITY_API_PHASE_ENTER) {
        uint64_t correlation_id = gpu_correlation_id();
        roctracer_correlation_id_push(correlation_id);
        cct_node_t *api_node = roctracer_correlation_callback(correlation_id);
        gpu_op_ccts_t gpu_op_ccts;
        hpcrun_safe_enter();
        gpu_op_ccts_insert(api_node, &gpu_op_ccts, gpu_op_placeholder_flags);
        hpcrun_safe_exit();
        
        // Generate notification entry
        uint64_t cpu_submit_time = CPU_NANOTIME();
        gpu_correlation_channel_produce(correlation_id, &gpu_op_ccts, cpu_submit_time);
    } else if (data->phase == ACTIVITY_API_PHASE_EXIT) {
        uint64_t correlation_id;
        roctracer_correlation_id_pop(&correlation_id);
    }
}

void
roctracer_buffer_completion_callback
(
      const char* begin,
      const char* end,
      void* arg
)
{
    correlations_consume();
    roctracer_record_t* record = (roctracer_record_t*)(begin);
    roctracer_record_t* end_record = (roctracer_record_t*)(end);
    while (record < end_record)
    {
        roctracer_activity_process(record);
//        roctracer_next_record(record, &record);
        record++;
    }
}

const char *
roctracer_path
(
  void
)
{
  const char *path = "libroctracer64.so";
  return path;
}

int
roctracer_bind
(
  void
)
{
#ifndef HPCRUN_STATIC_LINK
  // dynamic libraries only availabile in non-static case
  hpcrun_force_dlopen(true);
  CHK_DLOPEN(roctracer, roctracer_path(), RTLD_NOW | RTLD_GLOBAL);
  hpcrun_force_dlopen(false);

#define ROCTRACER_BIND(fn) \
  CHK_DLSYM(roctracer, fn);

  FORALL_ROCTRACER_ROUTINES(ROCTRACER_BIND)

#undef ROCTRACER_BIND

  return 0;
#else
  return -1;
#endif // ! HPCRUN_STATIC_LINK
}

void
roctracer_init
(

)
{
    roctracer_properties_t properties;
    properties.buffer_size = 0x1000;
    properties.buffer_callback_fun = roctracer_buffer_completion_callback;
    properties.mode = 0;
    properties.alloc_fun = 0;
    properties.alloc_arg = 0;
    properties.buffer_callback_arg = 0;
    HPCRUN_ROCTRACER_CALL(roctracer_open_pool,(&properties, NULL));
    HPCRUN_ROCTRACER_CALL(roctracer_enable_callback,(roctracer_subscriber_callback, NULL));
    HPCRUN_ROCTRACER_CALL(roctracer_enable_activity,(NULL));
}

void
roctracer_fini
(

)
{
    HPCRUN_ROCTRACER_CALL(roctracer_disable_callback,());
    HPCRUN_ROCTRACER_CALL(roctracer_disable_activity,());
    HPCRUN_ROCTRACER_CALL(roctracer_flush_activity,(NULL));
}

