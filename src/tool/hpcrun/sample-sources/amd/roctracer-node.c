//
// Created by user on 17.8.2019..
//

#include "roctracer-node.h"
#include <hpcrun/memory/hpcrun-malloc.h>



void
roctracer_activity_entry_set
(
     entry_activity_t *entry,
     roctracer_record_t *activity,
     cct_node_t *cct_node,
     entry_data_t *data
)
{
    entry->cct_node = cct_node;
    entry->roctracer_kind.op = activity->op;
    entry->roctracer_kind.domain = activity->domain;
    entry->data = data;
    if (entry->data == NULL) {
        entry->data = (entry_data_t *)hpcrun_malloc_safe(sizeof(entry_data_t));
    }
    switch(activity->domain)
    {
        case ACTIVITY_DOMAIN_HIP_API:
            switch(activity->op)
            {
                case HIP_API_ID_hipModuleLaunchKernel:
                case HIP_API_ID_hipLaunchCooperativeKernel:
                case HIP_API_ID_hipLaunchCooperativeKernelMultiDevice:
                case HIP_API_ID_hipHccModuleLaunchKernel:
                case HIP_API_ID_hipExtModuleLaunchKernel:
                    entry->data->kernel.start = activity->begin_ns;
                    entry->data->kernel.end = activity->end_ns;
                    break;
                case HIP_API_ID_hipCtxSynchronize:
                    entry->data->synchronization.syncKind = 4;
                    entry->data->synchronization.start = activity->begin_ns;
                    entry->data->synchronization.end = activity->end_ns;
                    break;
                case HIP_API_ID_hipStreamSynchronize:
                    entry->data->synchronization.syncKind = 3;
                    entry->data->synchronization.start = activity->begin_ns;
                    entry->data->synchronization.end = activity->end_ns;
                    break;
                case HIP_API_ID_hipStreamWaitEvent:
                    entry->data->synchronization.syncKind = 2;
                    entry->data->synchronization.start = activity->begin_ns;
                    entry->data->synchronization.end = activity->end_ns;
                    break;
                case HIP_API_ID_hipDeviceSynchronize:
                    break;
                case HIP_API_ID_hipEventSynchronize:
                    entry->data->synchronization.syncKind = 1;
                    entry->data->synchronization.start = activity->begin_ns;
                    entry->data->synchronization.end = activity->end_ns;
                    break;
                case HIP_API_ID_hipMemset3DAsync:
                case HIP_API_ID_hipMemset2D:
                case HIP_API_ID_hipMemset2DAsync:
                case HIP_API_ID_hipMemset:
                case HIP_API_ID_hipMemsetD8:
                case HIP_API_ID_hipMemset3D:
                case HIP_API_ID_hipMemsetAsync:
                case HIP_API_ID_hipMemsetD32Async:
                    entry->data->memset.start = activity->begin_ns;
                    entry->data->memset.end = activity->end_ns;
                    break;
            }
            break;
        case ACTIVITY_DOMAIN_HCC_OPS:
            switch(activity->op)
            {
                case HSA_OP_ID_COPY:
                    entry->data->memcpy.start = activity->begin_ns;
                    entry->data->memcpy.end = activity->end_ns;
                    entry->data->memcpy.copyKind = activity->kind;
                    entry->data->memcpy.bytes = activity->bytes;
                    break;
            }
            break;

        default:
            break;
    }
}
