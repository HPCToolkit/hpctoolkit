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
#include "roctracer-record.h"
#include "hip-state-placeholders.h"
#include <lib/prof-lean/stdatomic.h>
#include <lib/prof-lean/spinlock.h>
#include <lib/prof-lean/stdatomic.h>
#include <hpcrun/sample-sources/gpu/gpu-host-op-map.h>

#include <hpcrun/cct2metrics.h>
#include <hpcrun/files.h>
#include <hpcrun/hpcrun_stats.h>
#include <hpcrun/module-ignore-map.h>
#include <hpcrun/main.h>
#include <hpcrun/safe-sampling.h>
#include <hpcrun/sample_event.h>
#include <hpcrun/sample-sources/libdl.h>
#include <hpcrun/cct/cct.h>
#include <hpcrun/sample-sources/gpu/gpu-api.h>

#include <roctracer_hip.h>
#include <roctracer_hcc.h>

//static atomic_long roctracer_correlation_id = ATOMIC_VAR_INIT(1);



void
roctracer_activity_handle
(
        entry_activity_t *entry
)
{
    roctracer_activity_attribute(entry,
                             entry->cct_node);
}



static void
roctracer_correlation_callback
(
        uint64_t correlation_id,
        placeholder_t hip_state,
        entry_data_t *entry_data

)
{
    //PRINT("enter roctracer_correlation_callback %u\n", *id);
    gpu_correlation_callback(correlation_id, hip_state, entry_data);

    //PRINT("exit cupti_correlation_callback_cuda\n");

}

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

static void
roctracer_memset_data_set
(
        const hip_api_data_t *data,
        entry_data_t *entry_data,
        uint32_t callback_id
)
{
    switch(callback_id)
    {
        case HIP_API_ID_hipMemset2D:
            entry_data->memset.bytes = data->args.hipMemset2D.width * data->args.hipMemset2D.height;
            break;
        case HIP_API_ID_hipMemset2DAsync:
            entry_data->memset.bytes = data->args.hipMemset2DAsync.width * data->args.hipMemset2DAsync.height;
            break;
        case HIP_API_ID_hipMemset:
            entry_data->memset.bytes = data->args.hipMemset.sizeBytes;
            break;
        case HIP_API_ID_hipMemsetD8:
            entry_data->memset.bytes = data->args.hipMemsetD8.sizeBytes;
            break;
        /*case HIP_API_ID_hipMemset3D:
            entry_data->memset.bytes = data->args.hipMemset3D.
            break;*/
        case HIP_API_ID_hipMemsetAsync:
            entry_data->memset.bytes = data->args.hipMemsetAsync.sizeBytes;
            break;
       /* case HIP_API_ID_hipMemsetD32Async:
            entry_data->memset.bytes = data->args.hipMemsetD32Async.
            break;*/
    }
}

static void
roctracer_memcpy_data_set
(
      const hip_api_data_t *data,
      entry_data_t *entry_data,
      uint32_t callback_id
)
{
    switch(callback_id){
        case HIP_API_ID_hipMemcpyDtoD:
            entry_data->memcpy.copyKind = 8;
            entry_data->memcpy.bytes = data->args.hipMemcpyDtoD.sizeBytes;
            break;
        case HIP_API_ID_hipMemcpyAtoH:
            entry_data->memcpy.copyKind = 4;
            break;
        case HIP_API_ID_hipMemcpyHtoD:
            entry_data->memcpy.copyKind = 1;
            entry_data->memcpy.bytes = data->args.hipMemcpyHtoD.sizeBytes;
            break;
        case HIP_API_ID_hipMemcpyHtoA:
            entry_data->memcpy.copyKind = 3;
            break;
        case HIP_API_ID_hipMemcpyDtoH:
            entry_data->memcpy.copyKind = 2;
            entry_data->memcpy.bytes = data->args.hipMemcpyDtoH.sizeBytes;
            break;
        case HIP_API_ID_hipMemcpy:
            entry_data->memcpy.copyKind = data->args.hipMemcpy.kind;
            entry_data->memcpy.bytes = data->args.hipMemcpy.sizeBytes;
            break;
    }


}

void
roctracer_subscriber_callback
(
      uint32_t domain,
      uint32_t callback_id,
      const void* callback_data,
      void* arg
)
{
    gpu_record_init();
    placeholder_t hip_state = hip_placeholders.hip_none_state;
    const hip_api_data_t* data = (const hip_api_data_t*)(callback_data);
    entry_data_t *entry_data;
    if (data->phase == ACTIVITY_API_PHASE_ENTER) {
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
                hip_state = hip_placeholders.hip_memcpy_state;
                activities_consume(roctracer_activity_handle);
                entry_data = (entry_data_t*)hpcrun_malloc_safe(sizeof(entry_data_t));
                roctracer_memcpy_data_set(data, entry_data, callback_id);
                roctracer_correlation_callback(data->correlation_id, hip_state, NULL);
                break;
            case HIP_API_ID_hipMalloc:
            case HIP_API_ID_hipMallocPitch:
            case HIP_API_ID_hipMalloc3DArray:
            case HIP_API_ID_hipMallocArray:
            case HIP_API_ID_hipHostMalloc:
            case HIP_API_ID_hipMallocManaged:
            case HIP_API_ID_hipMalloc3D:
            case HIP_API_ID_hipExtMallocWithFlags:
                hip_state = hip_placeholders.hip_memalloc_state;
                activities_consume(roctracer_activity_handle);
                roctracer_correlation_callback(data->correlation_id, hip_state, NULL);
                break;
            case HIP_API_ID_hipMemset3DAsync:
            case HIP_API_ID_hipMemset2D:
            case HIP_API_ID_hipMemset2DAsync:
            case HIP_API_ID_hipMemset:
            case HIP_API_ID_hipMemsetD8:
            case HIP_API_ID_hipMemset3D:
            case HIP_API_ID_hipMemsetAsync:
            case HIP_API_ID_hipMemsetD32Async:
                hip_state = hip_placeholders.hip_memset_state;
                activities_consume(roctracer_activity_handle);
                entry_data = (entry_data_t*)hpcrun_malloc_safe(sizeof(entry_data_t));
                roctracer_memset_data_set(data, entry_data, callback_id);
                roctracer_correlation_callback(data->correlation_id, hip_state, entry_data);
                break;
            case HIP_API_ID_hipFree:
            case HIP_API_ID_hipFreeArray:
                hip_state = hip_placeholders.hip_free_state;
                activities_consume(roctracer_activity_handle);
                roctracer_correlation_callback(data->correlation_id, hip_state, NULL);
                break;
            case HIP_API_ID_hipModuleLaunchKernel:
            case HIP_API_ID_hipLaunchCooperativeKernel:
            case HIP_API_ID_hipHccModuleLaunchKernel:
            //case HIP_API_ID_hipExtModuleLaunchKernel:
                hip_state = hip_placeholders.hip_kernel_state;
                activities_consume(roctracer_activity_handle);
                entry_data = (entry_data_t*)hpcrun_malloc_safe(sizeof(entry_data_t));
                roctracer_kernel_data_set(data, entry_data, callback_id);
                roctracer_correlation_callback(data->correlation_id, hip_state, entry_data);
                break;
            case HIP_API_ID_hipCtxSynchronize:
            case HIP_API_ID_hipStreamSynchronize:
            case HIP_API_ID_hipDeviceSynchronize:
            case HIP_API_ID_hipEventSynchronize:
                hip_state = hip_placeholders.hip_sync_state;
                activities_consume(roctracer_activity_handle);
                roctracer_correlation_callback(data->correlation_id, hip_state, NULL);
            default:
                break;
        }
    }
}

static void
roctracer_kernel_launch_process
(
    roctracer_record_t *activity
)
{
    gpu_host_op_map_entry_t *host_op_entry = gpu_host_op_map_lookup(activity->correlation_id);
    if (host_op_entry != NULL) {
        entry_data_t *data = gpu_host_op_map_entry_data_get(host_op_entry);
        cct_node_t *host_op_node =
                gpu_host_op_map_entry_host_op_node_get(host_op_entry);
        gpu_record_t *record =
                gpu_host_op_map_entry_record_get(host_op_entry);
        roctracer_activity_produce(activity, host_op_node, record, data);
        // do not delete it because it shares external_id with activity samples
    }
}

static void
roctracer_memcpy_process
(
     roctracer_record_t *activity
)
{
    gpu_host_op_map_entry_t *host_op_entry = gpu_host_op_map_lookup(activity->correlation_id);
    if (host_op_entry != NULL) {
        entry_data_t *data = gpu_host_op_map_entry_data_get(host_op_entry);
        cct_node_t *host_op_node =
                gpu_host_op_map_entry_host_op_node_get(host_op_entry);
        gpu_record_t *record =
                gpu_host_op_map_entry_record_get(host_op_entry);
        roctracer_activity_produce(activity, host_op_node, record, data);
    }
}

static void
roctracer_memset_process
(
    roctracer_record_t *activity
)
{
    gpu_host_op_map_entry_t *host_op_entry = gpu_host_op_map_lookup(activity->correlation_id);
    if (host_op_entry != NULL) {
        entry_data_t *data = gpu_host_op_map_entry_data_get(host_op_entry);
        cct_node_t *host_op_node =
                gpu_host_op_map_entry_host_op_node_get(host_op_entry);
        gpu_record_t *record =
                gpu_host_op_map_entry_record_get(host_op_entry);
        roctracer_activity_produce(activity, host_op_node, record, data);
    }
}

/*static void
roctracer_free_process
(
     roctracer_record_t *activity
)
{
    // no impl in cupti
    gpu_host_op_map_entry_t *host_op_entry = gpu_host_op_map_lookup(activity->correlation_id);
    if (host_op_entry != NULL) {
        entry_data_t *data = gpu_host_op_map_entry_data_get(host_op_entry);
        cct_node_t *host_op_node =
                gpu_host_op_map_entry_host_op_node_get(host_op_entry);
        gpu_record_t *record =
                gpu_host_op_map_entry_record_get(host_op_entry);
        roctracer_activity_produce(activity, host_op_node, record, data);
        // do not delete it because it shares external_id with activity samples
    }
}*/

static void
roctracer_sync_process
(
     roctracer_record_t *activity
)
{
    gpu_host_op_map_entry_t *host_op_entry = gpu_host_op_map_lookup(activity->correlation_id);
    if (host_op_entry != NULL) {
        entry_data_t *data = gpu_host_op_map_entry_data_get(host_op_entry);
        cct_node_t *host_op_node =
                gpu_host_op_map_entry_host_op_node_get(host_op_entry);
        gpu_record_t *record =
                gpu_host_op_map_entry_record_get(host_op_entry);
        roctracer_activity_produce(activity, host_op_node, record, data);

    }
}

static void
roctracer_hc_memcpy_process
(
    roctracer_record_t *activity
)
{
    gpu_host_op_map_entry_t *host_op_entry = gpu_host_op_map_lookup(activity->correlation_id);
    if (host_op_entry != NULL) {
        entry_data_t *data = gpu_host_op_map_entry_data_get(host_op_entry);
        cct_node_t *host_op_node =
                gpu_host_op_map_entry_host_op_node_get(host_op_entry);
        gpu_record_t *record =
                gpu_host_op_map_entry_record_get(host_op_entry);
        roctracer_activity_produce(activity, host_op_node, record, data);

    }
}

static void
roctracer_activity_process
(
    roctracer_record_t *record
)
{
    const char * name = roctracer_op_string(record->domain, record->op, record->kind);
    if (record->domain == ACTIVITY_DOMAIN_HIP_API) {
        switch(record->op){
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
                roctracer_memcpy_process(record);
                break;
            case HIP_API_ID_hipModuleLaunchKernel:
            case HIP_API_ID_hipLaunchCooperativeKernel:
            case HIP_API_ID_hipHccModuleLaunchKernel:
            case HIP_API_ID_hipExtModuleLaunchKernel:
                roctracer_kernel_launch_process(record);
                break;
            case HIP_API_ID_hipCtxSynchronize:
            case HIP_API_ID_hipStreamSynchronize:
            case HIP_API_ID_hipDeviceSynchronize:
            case HIP_API_ID_hipEventSynchronize:
                roctracer_sync_process(record);
                break;
            case HIP_API_ID_hipMemset3DAsync:
            case HIP_API_ID_hipMemset2D:
            case HIP_API_ID_hipMemset2DAsync:
            case HIP_API_ID_hipMemset:
            case HIP_API_ID_hipMemsetD8:
            case HIP_API_ID_hipMemset3D:
            case HIP_API_ID_hipMemsetAsync:
            case HIP_API_ID_hipMemsetD32Async:
                roctracer_memset_process(record);
                break;

        }
    } else if (record->domain == ACTIVITY_DOMAIN_HCC_OPS) {
        if (record->op == HSA_OP_ID_COPY){
            roctracer_hc_memcpy_process(record);
        }
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


