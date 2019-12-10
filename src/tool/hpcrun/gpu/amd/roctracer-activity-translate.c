#include "roctracer-activity-translate.h"

#define DEBUG 0

#include <hpcrun/gpu/gpu-print.h>


// HSA_OP_ID_COPY is defined in hcc/include/hc_prof_runtime.h.
// However, this file will include many C++ code, making it impossible
// to compile with pure C.
// HSA_OP_ID_COPY is a constant with value 1 at the moment. 
#define HSA_OP_ID_COPY  1

static void
convert_kernel_launch
(
    gpu_activity_t *ga,
    roctracer_record_t *activity
)
{
    ga->kind = GPU_ACTIVITY_KERNEL;
    set_gpu_interval(&ga->details.interval, activity->begin_ns, activity->end_ns);
    ga->details.kernel.correlation_id = activity->correlation_id;
}

static void
convert_memcpy
(
    gpu_activity_t *ga,
    roctracer_record_t *activity,
    gpu_memcpy_type_t kind
)
{
    ga->kind = GPU_ACTIVITY_MEMCPY;
    set_gpu_interval(&ga->details.interval, activity->begin_ns, activity->end_ns);
    ga->details.memcpy.correlation_id = activity->correlation_id;
    ga->details.memcpy.copyKind = kind;
}


static void
convert_memset
(
    gpu_activity_t *ga,
    roctracer_record_t *activity
)
{
    ga->kind = GPU_ACTIVITY_MEMSET;
    set_gpu_interval(&ga->details.interval, activity->begin_ns, activity->end_ns);
    ga->details.memset.correlation_id = activity->correlation_id;
}

static void
convert_sync
(
    gpu_activity_t *ga,
    roctracer_record_t *activity,
    gpu_sync_type_t syncKind
)
{
    ga->kind = GPU_ACTIVITY_SYNCHRONIZATION;
    set_gpu_interval(&ga->details.interval, activity->begin_ns, activity->end_ns);
    ga->details.synchronization.syncKind = syncKind;
    ga->details.synchronization.correlation_id = activity->correlation_id;
}

static void
convert_unknown
(
    gpu_activity_t *ga
)
{
    ga->kind = GPU_ACTIVITY_UNKNOWN;
}

void
roctracer_activity_translate
(
 gpu_activity_t *ga,
 roctracer_record_t *record   
)
{
    const char * name = roctracer_op_string(record->domain, record->op, record->kind);        
    if (record->domain == ACTIVITY_DOMAIN_HIP_API) {
        switch(record->op){
            case HIP_API_ID_hipMemcpyDtoD:
            case HIP_API_ID_hipMemcpyDtoDAsync:
                convert_memcpy(ga, record, GPU_MEMCPY_D2D);
                break;
            case HIP_API_ID_hipMemcpy2DToArray:
                convert_memcpy(ga, record, GPU_MEMCPY_D2D);
                break;
            case HIP_API_ID_hipMemcpyAtoH:
                convert_memcpy(ga, record, GPU_MEMCPY_A2H);
                break;
            case HIP_API_ID_hipMemcpyHtoD:
            case HIP_API_ID_hipMemcpyHtoDAsync:
                convert_memcpy(ga, record, GPU_MEMCPY_H2D);
                break;
            case HIP_API_ID_hipMemcpyHtoA:
                convert_memcpy(ga, record, GPU_MEMCPY_H2A);
                break;
            case HIP_API_ID_hipMemcpyDtoH:
            case HIP_API_ID_hipMemcpyDtoHAsync:
                convert_memcpy(ga, record, GPU_MEMCPY_D2H);
                break;
            case HIP_API_ID_hipMemcpyAsync:
            case HIP_API_ID_hipMemcpyFromSymbol:
            case HIP_API_ID_hipMemcpy3D:
            case HIP_API_ID_hipMemcpy2D:
            case HIP_API_ID_hipMemcpyPeerAsync:
            case HIP_API_ID_hipMemcpyFromArray:
            case HIP_API_ID_hipMemcpy2DAsync:
            case HIP_API_ID_hipMemcpyToArray:
            case HIP_API_ID_hipMemcpyToSymbol:
            case HIP_API_ID_hipMemcpyPeer:
            case HIP_API_ID_hipMemcpyParam2D:
            case HIP_API_ID_hipMemcpy:
            case HIP_API_ID_hipMemcpyToSymbolAsync:
            case HIP_API_ID_hipMemcpyFromSymbolAsync:
                convert_memcpy(ga, record, GPU_MEMCPY_UNK);
                break;
            case HIP_API_ID_hipModuleLaunchKernel:
            case HIP_API_ID_hipLaunchCooperativeKernel:
            case HIP_API_ID_hipHccModuleLaunchKernel:
            case HIP_API_ID_hipExtModuleLaunchKernel:
                convert_kernel_launch(ga, record);
                break;
            case HIP_API_ID_hipCtxSynchronize:
                convert_sync(ga, record, GPU_SYNC_UNKNOWN);
                break;            
            case HIP_API_ID_hipStreamSynchronize:
                convert_sync(ga, record, GPU_SYNC_STREAM);
                break;            
            case HIP_API_ID_hipStreamWaitEvent:
                convert_sync(ga, record, GPU_SYNC_STREAM_EVENT_WAIT);
                break;            
            case HIP_API_ID_hipDeviceSynchronize:
                convert_sync(ga, record, GPU_SYNC_UNKNOWN);
                break;            
            case HIP_API_ID_hipEventSynchronize:
                convert_sync(ga, record, GPU_SYNC_EVENT);
                break;
            case HIP_API_ID_hipMemset3DAsync:
            case HIP_API_ID_hipMemset2D:
            case HIP_API_ID_hipMemset2DAsync:
            case HIP_API_ID_hipMemset:
            case HIP_API_ID_hipMemsetD8:
            case HIP_API_ID_hipMemset3D:
            case HIP_API_ID_hipMemsetAsync:
            case HIP_API_ID_hipMemsetD32Async:
                convert_memset(ga, record);
                break;
            default:
                convert_unknown(ga);
                PRINT("Unhandled HIP activity %s\n", name);
        }
    } else if (record->domain == ACTIVITY_DOMAIN_HCC_OPS) {
        if (record->op == HSA_OP_ID_COPY){
            convert_memcpy(ga, record, GPU_MEMCPY_UNK);
        } else {
            convert_unknown(ga);
            PRINT("Unhandled HIP activity %s\n", name);
        }
    } else {
        convert_unknown(ga);
        PRINT("Unhandled HIP activity %s\n", name);
    }
    cstack_ptr_set(&(ga->next), 0);
}