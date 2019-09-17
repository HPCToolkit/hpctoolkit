//
// Created by user on 17.8.2019..
//

#include "amd.h"
#include "roctracer-api.h"

#include "../simple_oo.h"
#include "../sample_source_obj.h"
#include "../common.h"
#include <hpcrun/hpcrun_options.h>
#include <hpcrun/hpcrun_stats.h>
#include <hpcrun/metrics.h>
#include <hpcrun/sample_sources_registered.h>
#include <hpcrun/sample_event.h>
#include <hpcrun/thread_data.h>
#include <hpcrun/module-ignore-map.h>
#include <hpcrun/device-finalizers.h>
#include <hpcrun/safe-sampling.h>
#include <hpcrun/control-knob.h>
#include <utilities/tokenize.h>
#include <messages/messages.h>
#include <lush/lush-backtrace.h>
#include <lib/prof-lean/hpcrun-fmt.h>
#include <ompt/ompt-interface.h>
#include <roctracer_hip.h>
#include <roctracer_hcc.h>

#define FORALL_SYNC(macro) \
  macro("SYNC:UNKNOWN (us)",     0) \
  macro("SYNC:EVENT (us)",       1) \
  macro("SYNC:STREAM_WAIT (us)", 2) \
  macro("SYNC:STREAM (us)",      3) \
  macro("SYNC:CONTEXT (us)",     4)

#define FORALL_ME_SET(macro) \
  macro("MEM_SET:UNKNOWN_BYTES",       0) \
  macro("MEM_SET:PAGEABLE_BYTES",      1) \
  macro("MEM_SET:PINNED_BYTES",        2) \
  macro("MEM_SET:DEVICE_BYTES",        3) \
  macro("MEM_SET:ARRAY_BYTES",         4) \
  macro("MEM_SET:MANAGED_BYTES",       5) \
  macro("MEM_SET:DEVICE_STATIC_BYTES", 6)

#define FORALL_EM(macro) \
  macro("XDMOV:INVALID",       0)	\
  macro("XDMOV:HTOD_BYTES",    1)	\
  macro("XDMOV:DTOH_BYTES",    2)	\
  macro("XDMOV:HTOA_BYTES",    3)	\
  macro("XDMOV:ATOH_BYTES",    4)	\
  macro("XDMOV:ATOA_BYTES",    5)	\
  macro("XDMOV:ATOD_BYTES",    6)	\
  macro("XDMOV:DTOA_BYTES",    7)	\
  macro("XDMOV:DTOD_BYTES",    8)	\
  macro("XDMOV:HTOH_BYTES",    9)	\
  macro("XDMOV:PTOP_BYTES",   10)

#define COUNT_FORALL_CLAUSE(a,b) + 1
#define NUM_CLAUSES(forall_macro) 0 forall_macro(COUNT_FORALL_CLAUSE)


static int ke_static_shared_metric_id;
static int ke_dynamic_shared_metric_id;
static int ke_local_metric_id;
static int ke_active_warps_per_sm_metric_id;
static int ke_max_active_warps_per_sm_metric_id;
static int ke_thread_registers_id;
static int ke_block_threads_id;
static int ke_block_shared_memory_id;
static int ke_count_metric_id;
static int ke_time_metric_id;

//synchronization
static int sync_metric_id[NUM_CLAUSES(FORALL_SYNC)+1];
static int sync_time_metric_id;

//memset
static int me_set_metric_id[NUM_CLAUSES(FORALL_ME_SET)+1];
static int me_set_time_metric_id;

//memcpy
static int em_metric_id[NUM_CLAUSES(FORALL_EM)+1];
static int em_time_metric_id;

void roctracer_activity_attribute
(
        entry_activity_t *activity,
        cct_node_t *cct_node
)
{
    thread_data_t *td = hpcrun_get_thread_data();
    td->overhead++;
    hpcrun_safe_enter();

    switch(activity->roctracer_kind.domain)
    {
        case ACTIVITY_DOMAIN_HIP_API:
            switch(activity->roctracer_kind.op)
            {
                case HIP_API_ID_hipModuleLaunchKernel:
                case HIP_API_ID_hipLaunchCooperativeKernel:
                case HIP_API_ID_hipLaunchCooperativeKernelMultiDevice:
                case HIP_API_ID_hipExtModuleLaunchKernel:
                case HIP_API_ID_hipHccModuleLaunchKernel: {
                    metric_data_list_t *metrics = hpcrun_reify_metric_set(cct_node, ke_static_shared_metric_id);
                    hpcrun_metric_std_inc(ke_static_shared_metric_id, metrics,
                                          (cct_metric_data_t) {.i = activity->data->kernel.staticSharedMemory});

                    metrics = hpcrun_reify_metric_set(cct_node, ke_dynamic_shared_metric_id);
                    hpcrun_metric_std_inc(ke_dynamic_shared_metric_id, metrics,
                                          (cct_metric_data_t) {.i = activity->data->kernel.dynamicSharedMemory});

                    metrics = hpcrun_reify_metric_set(cct_node, ke_local_metric_id);
                    hpcrun_metric_std_inc(ke_local_metric_id, metrics,
                                          (cct_metric_data_t) {.i = activity->data->kernel.localMemoryTotal});

                    metrics = hpcrun_reify_metric_set(cct_node, ke_active_warps_per_sm_metric_id);
                    hpcrun_metric_std_inc(ke_active_warps_per_sm_metric_id, metrics,
                                          (cct_metric_data_t) {.i = activity->data->kernel.activeWarpsPerSM});

                    metrics = hpcrun_reify_metric_set(cct_node, ke_max_active_warps_per_sm_metric_id);
                    hpcrun_metric_std_inc(ke_max_active_warps_per_sm_metric_id, metrics,
                                          (cct_metric_data_t) {.i = activity->data->kernel.maxActiveWarpsPerSM});

                    metrics = hpcrun_reify_metric_set(cct_node, ke_thread_registers_id);
                    hpcrun_metric_std_inc(ke_thread_registers_id, metrics,
                                          (cct_metric_data_t) {.i = activity->data->kernel.threadRegisters});

                    metrics = hpcrun_reify_metric_set(cct_node, ke_block_threads_id);
                    hpcrun_metric_std_inc(ke_block_threads_id, metrics,
                                          (cct_metric_data_t) {.i = activity->data->kernel.blockThreads});

                    metrics = hpcrun_reify_metric_set(cct_node, ke_block_shared_memory_id);
                    hpcrun_metric_std_inc(ke_block_shared_memory_id, metrics,
                                          (cct_metric_data_t) {.i = activity->data->kernel.blockSharedMemory});

                    metrics = hpcrun_reify_metric_set(cct_node, ke_count_metric_id);
                    hpcrun_metric_std_inc(ke_count_metric_id, metrics, (cct_metric_data_t) {.i = 1});

                    metrics = hpcrun_reify_metric_set(cct_node, ke_time_metric_id);
                    hpcrun_metric_std_inc(ke_time_metric_id, metrics, (cct_metric_data_t) {.r =
                    (activity->data->kernel.end - activity->data->kernel.start) / 1000.0});
                    break;
                }
                case HIP_API_ID_hipCtxSynchronize:
                case HIP_API_ID_hipStreamSynchronize:
                case HIP_API_ID_hipStreamWaitEvent:
                case HIP_API_ID_hipDeviceSynchronize:
                case HIP_API_ID_hipEventSynchronize: {
                    int index = sync_metric_id[activity->data->synchronization.syncKind];
                    metric_data_list_t *metrics = hpcrun_reify_metric_set(cct_node, index);
                    hpcrun_metric_std_inc(index, metrics, (cct_metric_data_t) {.r =
                    (activity->data->synchronization.end - activity->data->synchronization.start) / 1000.0});

                    metrics = hpcrun_reify_metric_set(cct_node, sync_time_metric_id);
                    hpcrun_metric_std_inc(sync_time_metric_id, metrics, (cct_metric_data_t) {.r =
                    (activity->data->synchronization.end - activity->data->synchronization.start) / 1000.0});
                    break;
                }
                case HIP_API_ID_hipMemset3DAsync:
                case HIP_API_ID_hipMemset2D:
                case HIP_API_ID_hipMemset2DAsync:
                case HIP_API_ID_hipMemset:
                case HIP_API_ID_hipMemsetD8:
                case HIP_API_ID_hipMemset3D:
                case HIP_API_ID_hipMemsetAsync:
                case HIP_API_ID_hipMemsetD32Async: {
                    int index = me_set_metric_id[activity->data->memset.memKind];
                    metric_data_list_t *metrics = hpcrun_reify_metric_set(cct_node, index);
                    hpcrun_metric_std_inc(index, metrics, (cct_metric_data_t) {.i = activity->data->memset.bytes});

                    metrics = hpcrun_reify_metric_set(cct_node, me_set_time_metric_id);
                    hpcrun_metric_std_inc(me_set_time_metric_id, metrics, (cct_metric_data_t) {.r =
                    (activity->data->memset.end - activity->data->memset.start) / 1000.0});
                    break;
                }
            }
        break;
    case ACTIVITY_DOMAIN_HCC_OPS:
        switch(activity->roctracer_kind.op)
        {
            case HSA_OP_ID_COPY: {
                int index = em_metric_id[activity->data->memcpy.copyKind];
                metric_data_list_t *metrics = hpcrun_reify_metric_set(cct_node, index);
                hpcrun_metric_std_inc(index, metrics, (cct_metric_data_t) {.i = activity->data->memcpy.bytes});

                metrics = hpcrun_reify_metric_set(cct_node, em_time_metric_id);
                hpcrun_metric_std_inc(em_time_metric_id, metrics, (cct_metric_data_t) {.r =
                (activity->data->memcpy.end - activity->data->memcpy.start) / 1000.0});
                break;
            }
        }
    break;

    default:
        break;
}

    hpcrun_safe_exit();
    td->overhead--;
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
    roctracer_open_pool(&properties, NULL);
    roctracer_enable_callback(roctracer_subscriber_callback, NULL);
    roctracer_enable_activity(NULL);
}

void
roctracer_fini
(

)
{
    roctracer_disable_callback();
    roctracer_disable_activity();
    roctracer_flush_activity(NULL);
}