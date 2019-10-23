//
// Created by user on 17.8.2019..
//

#include <alloca.h>
#include <assert.h>
#include <ctype.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ucontext.h>
#include <stdbool.h>

#include <pthread.h>

#ifndef HPCRUN_STATIC_LINK
#include <dlfcn.h>
#endif

/******************************************************************************
 * libmonitor
 *****************************************************************************/

#include <monitor.h>

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

#define FORALL_ME(macro) \
  macro("MEM_ALLOC:UNKNOWN_BYTES",       0) \
  macro("MEM_ALLOC:PAGEABLE_BYTES",      1) \
  macro("MEM_ALLOC:PINNED_BYTES",        2) \
  macro("MEM_ALLOC:DEVICE_BYTES",        3) \
  macro("MEM_ALLOC:ARRAY_BYTES",         4) \
  macro("MEM_ALLOC:MANAGED_BYTES",       5) \
  macro("MEM_ALLOC:DEVICE_STATIC_BYTES", 6)

#define FORALL_ME_TIME(macro) \
  macro("MEM_ALLOC:TIME (us)",           7)

#define FORALL_ME_SET(macro) \
  macro("MEM_SET:UNKNOWN_BYTES",       0) \
  macro("MEM_SET:PAGEABLE_BYTES",      1) \
  macro("MEM_SET:PINNED_BYTES",        2) \
  macro("MEM_SET:DEVICE_BYTES",        3) \
  macro("MEM_SET:ARRAY_BYTES",         4) \
  macro("MEM_SET:MANAGED_BYTES",       5) \
  macro("MEM_SET:DEVICE_STATIC_BYTES", 6)

#define FORALL_ME_SET_TIME(macro) \
  macro("MEM_SET:TIME (us)",           7)

#define FORALL_KE(macro) \
  macro("KERNEL:STATIC_MEM_BYTES",  0)       \
  macro("KERNEL:DYNAMIC_MEM_BYTES", 1)       \
  macro("KERNEL:LOCAL_MEM_BYTES",   2)       \
  macro("KERNEL:ACTIVE_WARPS_PER_SM", 3)     \
  macro("KERNEL:MAX_ACTIVE_WARPS_PER_SM", 4) \
  macro("KERNEL:THREAD_REGISTERS", 5)       \
  macro("KERNEL:BLOCK_THREADS", 6)           \
  macro("KERNEL:BLOCK_SHARED_MEMORY", 7)     \
  macro("KERNEL:COUNT ", 8)                  \

#define FORALL_KE_TIME(macro) \
  macro("KERNEL:TIME (us)", 9)

#define FORALL_KE_OCCUPANCY(macro) \
  macro("KERNEL:OCCUPANCY", 10)

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

#define FORALL_EM_TIME(macro) \
  macro("XDMOV:TIME (us)",    11)

#define FORALL_SYNC(macro) \
  macro("SYNC:UNKNOWN (us)",     0) \
  macro("SYNC:EVENT (us)",       1) \
  macro("SYNC:STREAM_WAIT (us)", 2) \
  macro("SYNC:STREAM (us)",      3) \
  macro("SYNC:CONTEXT (us)",     4)

#define FORALL_SYNC_TIME(macro) \
  macro("SYNC:TIME (us)",     5)

#define FORALL_GL(macro) \
  macro("GMEM:LOAD_CACHED_BYTES",             0) \
  macro("GMEM:LOAD_UNCACHED_BYTES",           1) \
  macro("GMEM:STORE_BYTES",                   2) \
  macro("GMEM:LOAD_CACHED_L2_TRANS",          3) \
  macro("GMEM:LOAD_UNCACHED_L2_TRANS",        4) \
  macro("GMEM:STORE_L2_TRANS",                5) \
  macro("GMEM:LOAD_CACHED_THEORETIC_TRANS",   6) \
  macro("GMEM:LOAD_UNCACHED_THEORETIC_TRANS", 7) \
  macro("GMEM:STORE_THEORETIC_TRANS",         8)

#define FORALL_SH(macro) \
  macro("SMEM:LOAD_BYTES",            0) \
  macro("SMEM:STORE_BYTES",           1) \
  macro("SMEM:LOAD_TRANS",            2) \
  macro("SMEM:STORE_TRANS",           3) \
  macro("SMEM:LOAD_THEORETIC_TRANS",  4) \
  macro("SMEM:STORE_THEORETIC_TRANS", 5)

#define FORALL_BH(macro) \
  macro("BRANCH:WARP_DIVERGED", 0) \
  macro("BRANCH:WARP_EXECUTED", 1)

#define FORALL_INFO(macro) \
  macro("KERNEL:DROPPED_SAMPLES",  0) \
  macro("KERNEL:PERIOD_IN_CYCLES", 1) \
  macro("KERNEL:TOTAL_SAMPLES",    2) \
  macro("KERNEL:SM_FULL_SAMPLES",  3)

#define FORALL_INFO_EFFICIENCY(macro) \
  macro("KERNEL:SM_EFFICIENCY", 4)

#if CUPTI_API_VERSION >= 10
#define FORALL_IM(macro) \
  macro("IDMOV:INVALID",       0)	\
  macro("IDMOV:HTOD_BYTES",    1)	\
  macro("IDMOV:DTOH_BYTES",    2)	\
  macro("IDMOV:CPU_PF",        3)	\
  macro("IDMOV:GPU_PF",        4)	\
  macro("IDMOV:THRASH",        5)	\
  macro("IDMOV:THROT",         6)	\
  macro("IDMOV:RMAP",          7)	\
  macro("IDMOV:DTOD_BYTES",    8)

#define FORALL_IM_TIME(macro) \
  macro("IDMOV:TIME (us)",     9)
#else
#define FORALL_IM(macro) \
  macro("IDMOV:INVALID",       0)	\
  macro("IDMOV:HTOD_BYTES",    1)	\
  macro("IDMOV:DTOH_BYTES",    2)	\
  macro("IDMOV:CPU_PF",        3)	\
  macro("IDMOV:GPU_PF",        4)

#define FORALL_IM_TIME(macro) \
  macro("IDMOV:TIME (us)",     5)
#endif

#if CUPTI_API_VERSION >= 10
#define FORALL_STL(macro)	\
  macro("STALL:INVALID",      0) \
  macro("STALL:NONE",         1) \
  macro("STALL:IFETCH",       2) \
  macro("STALL:EXC_DEP",      3) \
  macro("STALL:MEM_DEP",      4) \
  macro("STALL:TEX",          5) \
  macro("STALL:SYNC",         6) \
  macro("STALL:CMEM_DEP",     7) \
  macro("STALL:PIPE_BSY",     8) \
  macro("STALL:MEM_THR",      9) \
  macro("STALL:NOSEL",       10) \
  macro("STALL:OTHR",        11) \
  macro("STALL:SLEEP",       12)

#define FORALL_GPU_INST(macro) \
  macro("GPU INST",          13)

#define FORALL_GPU_INST_LAT(macro) \
  macro("GPU STALL",         14)
#else
#define FORALL_STL(macro)	\
  macro("STALL:INVALID",      0) \
  macro("STALL:NONE",         1) \
  macro("STALL:IFETCH",       2) \
  macro("STALL:EXC_DEP",      3) \
  macro("STALL:MEM_DEP",      4) \
  macro("STALL:TEX",          5) \
  macro("STALL:SYNC",         6) \
  macro("STALL:CMEM_DEP",     7) \
  macro("STALL:PIPE_BSY",     8) \
  macro("STALL:MEM_THR",      9) \
  macro("STALL:NOSEL",       10) \
  macro("STALL:OTHR",        11)

#define FORALL_GPU_INST(macro) \
  macro("GPU INST",       12)

#define FORALL_GPU_INST_LAT(macro) \
  macro("GPU STALL",   13)
#endif


#define COUNT_FORALL_CLAUSE(a,b) + 1
#define NUM_CLAUSES(forall_macro) 0 forall_macro(COUNT_FORALL_CLAUSE)

static device_finalizer_fn_entry_t device_finalizer_flush;
static device_finalizer_fn_entry_t device_finalizer_shutdown;

static kind_info_t* stall_kind; // gpu insts
static kind_info_t* ke_kind; // kernel execution
static kind_info_t* em_kind; // explicit memory copies
static kind_info_t* im_kind; // implicit memory events
static kind_info_t* me_kind; // memory allocation and deletion
static kind_info_t* me_set_kind; // memory set
static kind_info_t* gl_kind; // global memory accesses
static kind_info_t* sh_kind; // shared memory accesses
static kind_info_t* bh_kind; // branches
static kind_info_t* sync_kind; // stream, context, or event sync
static kind_info_t* info_kind; // pc sampling info

static int stall_metric_id[NUM_CLAUSES(FORALL_STL)+2];
static int gpu_inst_metric_id;
static int gpu_inst_lat_metric_id;

static int em_metric_id[NUM_CLAUSES(FORALL_EM)+1];
static int em_time_metric_id;

static int im_metric_id[NUM_CLAUSES(FORALL_IM)+1];
static int im_time_metric_id;

static int me_metric_id[NUM_CLAUSES(FORALL_ME)+1];
static int me_time_metric_id;

static int me_set_metric_id[NUM_CLAUSES(FORALL_ME_SET)+1];
static int me_set_time_metric_id;

static int gl_metric_id[NUM_CLAUSES(FORALL_GL)];

static int sh_metric_id[NUM_CLAUSES(FORALL_SH)];

static int bh_metric_id[NUM_CLAUSES(FORALL_BH)];
static int bh_diverged_metric_id;
static int bh_executed_metric_id;

static int ke_metric_id[NUM_CLAUSES(FORALL_KE)+2];
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
static int ke_occupancy_metric_id;

static int info_metric_id[NUM_CLAUSES(FORALL_INFO)+1];
static int info_dropped_samples_id;
static int info_period_in_cycles_id;
static int info_total_samples_id;
static int info_sm_full_samples_id;
static int info_sm_efficiency_id;

static int sync_metric_id[NUM_CLAUSES(FORALL_SYNC)+1];
static int sync_time_metric_id;

#define AMD_ROCM "amd-rocm"
static char amd_name[128];

static const unsigned int MAX_CHAR_FORMULA = 32;

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
                case HIP_API_ID_hipMemcpy:
                case HIP_API_ID_hipMemcpyAtoH:
                case HIP_API_ID_hipMemcpyHtoD:
                case HIP_API_ID_hipMemcpyHtoA: {
                    int index = em_metric_id[activity->data->memcpy.copyKind];
                    metric_data_list_t *metrics = hpcrun_reify_metric_set(cct_node, index);
                    hpcrun_metric_std_inc(index, metrics, (cct_metric_data_t) {.i = activity->data->memcpy.bytes});

                    metrics = hpcrun_reify_metric_set(cct_node, em_time_metric_id);
                    hpcrun_metric_std_inc(em_time_metric_id, metrics, (cct_metric_data_t) {.r =
                    (activity->data->memcpy.end - activity->data->memcpy.start) / 1000.0});
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
    break;

    default:
        break;
}

    hpcrun_safe_exit();
    td->overhead--;
}

static void
METHOD_FN(init)
{
    self->state = INIT;
}

static void
METHOD_FN(thread_init)
{
    TMSG(CUDA, "thread_init");
}

static void
METHOD_FN(thread_init_action)
{
    TMSG(CUDA, "thread_init_action");
}
static void
METHOD_FN(start)
{
    TMSG(CUDA, "start");
}

static void
METHOD_FN(thread_fini_action)
{
    TMSG(CUDA, "thread_fini_action");
}

static void
METHOD_FN(stop)
{
    hpcrun_get_thread_data();

    TD_GET(ss_state)[self->sel_idx] = STOP;
}

static void
METHOD_FN(shutdown)
{
    self->state = UNINIT;
}


static bool
METHOD_FN(supports_event, const char *ev_str)
{
#ifndef HPCRUN_STATIC_LINK
    return hpcrun_ev_is(ev_str, AMD_ROCM);
#else
    return false;
#endif


}

static void
METHOD_FN(process_event_list, int lush_metrics)
{
    int nevents = (self->evl).nevents;

    TMSG(CUDA,"nevents = %d", nevents);

#define getindex(name, index) index

#define declare_cur_metrics(name, index) \
  cur_metrics[index] = hpcrun_set_new_metric_info(cur_kind, name);

#define declare_cur_metrics_real(name, index) \
  cur_metrics[index] = hpcrun_set_new_metric_info_and_period(cur_kind, name, \
    MetricFlags_ValFmt_Real, 1, metric_property_none);

#define hide_cur_metrics(name, index) \
  hpcrun_set_display(cur_metrics[index], 0);

#define create_cur_kind cur_kind = hpcrun_metrics_new_kind()
#define close_cur_kind hpcrun_close_kind(cur_kind)

#define cur_kind stall_kind
#define cur_metrics stall_metric_id

    create_cur_kind;
    // GPU INST must be the first kind for sample apportion
    FORALL_GPU_INST(declare_cur_metrics);
    FORALL_GPU_INST_LAT(declare_cur_metrics);
    FORALL_STL(declare_cur_metrics);
    FORALL_STL(hide_cur_metrics);
    gpu_inst_metric_id = stall_metric_id[FORALL_GPU_INST(getindex)];
    gpu_inst_lat_metric_id = stall_metric_id[FORALL_GPU_INST_LAT(getindex)];
    close_cur_kind;

#undef cur_kind
#undef cur_metrics

#define cur_kind im_kind
#define cur_metrics im_metric_id

    create_cur_kind;
    FORALL_IM(declare_cur_metrics);
    FORALL_IM(hide_cur_metrics);
    FORALL_IM_TIME(declare_cur_metrics_real);
    im_time_metric_id = im_metric_id[FORALL_IM_TIME(getindex)];
    close_cur_kind;

#undef cur_kind
#undef cur_metrics

#define cur_kind em_kind
#define cur_metrics em_metric_id

    create_cur_kind;
    FORALL_EM(declare_cur_metrics);
    FORALL_EM(hide_cur_metrics);
    FORALL_EM_TIME(declare_cur_metrics_real);
    em_time_metric_id = em_metric_id[FORALL_EM_TIME(getindex)];
    close_cur_kind;

#undef cur_kind
#undef cur_metrics

#define cur_kind me_kind
#define cur_metrics me_metric_id

    create_cur_kind;
    FORALL_ME(declare_cur_metrics);
    FORALL_ME(hide_cur_metrics);
    FORALL_ME_TIME(declare_cur_metrics_real);
    me_time_metric_id = me_metric_id[FORALL_ME_TIME(getindex)];
    close_cur_kind;

#undef cur_kind
#undef cur_metrics

#define cur_kind me_set_kind
#define cur_metrics me_set_metric_id

    create_cur_kind;
    FORALL_ME_SET(declare_cur_metrics);
    FORALL_ME_SET(hide_cur_metrics);
    FORALL_ME_SET_TIME(declare_cur_metrics_real);
    me_set_time_metric_id = me_set_metric_id[FORALL_ME_SET_TIME(getindex)];
    close_cur_kind;

#undef cur_kind
#undef cur_metrics

#define cur_kind ke_kind
#define cur_metrics ke_metric_id

    create_cur_kind;
    FORALL_KE(declare_cur_metrics);
    FORALL_KE(hide_cur_metrics);
    FORALL_KE_TIME(declare_cur_metrics_real);
    FORALL_KE_OCCUPANCY(declare_cur_metrics_real);
    ke_static_shared_metric_id = ke_metric_id[0];
    ke_dynamic_shared_metric_id = ke_metric_id[1];
    ke_local_metric_id = ke_metric_id[2];
    ke_active_warps_per_sm_metric_id = ke_metric_id[3];
    ke_max_active_warps_per_sm_metric_id = ke_metric_id[4];
    ke_thread_registers_id = ke_metric_id[5];
    ke_block_threads_id = ke_metric_id[6];
    ke_block_shared_memory_id = ke_metric_id[7];
    ke_count_metric_id = ke_metric_id[8];
    ke_time_metric_id = ke_metric_id[9];
    ke_occupancy_metric_id = ke_metric_id[10];

    hpcrun_set_percent(ke_occupancy_metric_id, 1);
    metric_desc_t* ke_occupancy_metric = hpcrun_id2metric_linked(ke_occupancy_metric_id);
    char *ke_occupancy_buffer = hpcrun_malloc_safe(sizeof(char) * MAX_CHAR_FORMULA);
    sprintf(ke_occupancy_buffer, "$%d/$%d", ke_active_warps_per_sm_metric_id, ke_max_active_warps_per_sm_metric_id);
    ke_occupancy_metric->formula = ke_occupancy_buffer;

    close_cur_kind;

#undef cur_kind
#undef cur_metrics

#define cur_kind sync_kind
#define cur_metrics sync_metric_id

    create_cur_kind;
    FORALL_SYNC(declare_cur_metrics_real);
    FORALL_SYNC(hide_cur_metrics);
    FORALL_SYNC_TIME(declare_cur_metrics_real);
    sync_time_metric_id = sync_metric_id[5];
    close_cur_kind;

#undef cur_kind
#undef cur_metrics

#define cur_kind gl_kind
#define cur_metrics gl_metric_id

    create_cur_kind;
    FORALL_GL(declare_cur_metrics);
    FORALL_GL(hide_cur_metrics);
    close_cur_kind;

#undef cur_kind
#undef cur_metrics

#define cur_kind sh_kind
#define cur_metrics sh_metric_id

    create_cur_kind;
    FORALL_SH(declare_cur_metrics);
    FORALL_SH(hide_cur_metrics);
    close_cur_kind;

#undef cur_kind
#undef cur_metrics

#define cur_kind bh_kind
#define cur_metrics bh_metric_id

    create_cur_kind;
    FORALL_BH(declare_cur_metrics);
    FORALL_BH(hide_cur_metrics);
    bh_diverged_metric_id = bh_metric_id[0];
    bh_executed_metric_id = bh_metric_id[1];
    close_cur_kind;

#undef cur_kind
#undef cur_metrics

#define cur_kind info_kind
#define cur_metrics info_metric_id

    create_cur_kind;
    FORALL_INFO(declare_cur_metrics);
    FORALL_INFO(hide_cur_metrics);
    FORALL_INFO_EFFICIENCY(declare_cur_metrics_real);
    info_dropped_samples_id = info_metric_id[0];
    info_period_in_cycles_id = info_metric_id[1];
    info_total_samples_id = info_metric_id[2];
    info_sm_full_samples_id = info_metric_id[3];
    info_sm_efficiency_id = info_metric_id[4];

    hpcrun_set_percent(info_sm_efficiency_id, 1);
    metric_desc_t* sm_efficiency_metric = hpcrun_id2metric_linked(info_sm_efficiency_id);
    char *sm_efficiency_buffer = hpcrun_malloc_safe(sizeof(char) * MAX_CHAR_FORMULA);
    sprintf(sm_efficiency_buffer, "$%d/$%d", info_total_samples_id, info_sm_full_samples_id);
    sm_efficiency_metric->formula = sm_efficiency_buffer;

    close_cur_kind;

#ifndef HPCRUN_STATIC_LINK
    if (roctracer_bind()) {
        EEMSG("hpcrun: unable to bind to AMD roctracer library %s\n", dlerror());
        monitor_real_exit(-1);
    }
#endif

    // Fetch the event string for the sample source
    // only one event is allowed
    char* evlist = METHOD_CALL(self, get_event_str);
    char* event = start_tok(evlist);
    if (hpcrun_ev_is(amd_name, AMD_ROCM)) {
        hip_init_placeholders();
    }    
    roctracer_init();
}

static void
METHOD_FN(gen_event_set,int lush_metrics)
{
}

static void
METHOD_FN(display_events)
{
    printf("\n");
}

/***************************************************************************
 * object
 ***************************************************************************/

#define ss_name amd_gpu
#define ss_cls SS_HARDWARE

#include "../ss_obj.h"

/******************************************************************************
 * private operations
 *****************************************************************************/
