// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://outreach.scidac.gov/svn/hpctoolkit/branches/hpctoolkit-gpu-blame-shift-proto/src/tool/hpcrun/sample-sources/gpu_blame.c $
// $Id: itimer.c 3784 2012-05-10 22:35:51Z mc29 $
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2011, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// **

//
// Blame shiting interface
//

/******************************************************************************
 * system includes
 *****************************************************************************/
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

#include <signal.h>
#include <sys/time.h>           /* setitimer() */
#include <ucontext.h>           /* struct ucontext */



/******************************************************************************
 * libmonitor
 *****************************************************************************/

#include <monitor.h>


/******************************************************************************
 * local includes
 *****************************************************************************/
#include "sample_source_obj.h"
#include "common.h"
#include <hpcrun/hpcrun_options.h>
#include <hpcrun/hpcrun_stats.h>

#include <hpcrun/metrics.h>
#include <hpcrun/safe-sampling.h>
#include <hpcrun/sample_event.h>
#include <hpcrun/sample_sources_registered.h>
#include <hpcrun/thread_data.h>
#include <hpcrun/trace.h>

#include <lush/lush-backtrace.h>
#include <messages/messages.h>

#include <utilities/tokenize.h>
#include <utilities/arch/context-pc.h>

#include <unwind/common/unwind.h>

#include <lib/support-lean/timer.h>
#include <lib/prof-lean/spinlock.h>
#include <lib/prof-lean/atomic.h>
#include <lib/prof-lean/splay-macros.h>

#ifdef ENABLE_CUDA
#include "gpu_blame.h"
#endif

// ******* Global Variables ***********

/*
cudaRuntimeFunctionPointer_t  cudaRuntimeFunctionPointer[] = {
    {0, "cudaThreadSynchronize"},
    {0, "cudaStreamSynchronize"},
    {0, "cudaEventSynchronize"},
    {0, "cudaStreamWaitEvent"},
    {0, "cudaDeviceSynchronize"},
    {0, "cudaConfigureCall"},
    {0, "cudaLaunch"},
    {0, "cudaStreamDestroy"},
    {0, "cudaStreamCreate"},
    {0, "cudaMalloc"},
    {0, "cudaFree"},
    {0, "cudaMemcpyAsync"},
    {0, "cudaMemcpy"},
    {0, "cudaMemcpy2D"},
    {0, "cudaEventElapsedTime"},
    {0, "cudaEventCreate"},
    {0, "cudaEventRecord"},
    {0, "cudaEventDestroy"}
    
};

cuDriverFunctionPointer_t cuDriverFunctionPointer[] = {
    {0, "cuStreamCreate"},
    {0, "cuStreamDestroy"},
    {0, "cuStreamSynchronize"},
    {0, "cuEventSynchronize"},
    {0, "cuLaunchGridAsync"},
    {0, "cuCtxDestroy"},
    {0, "cuEventCreate"},
    {0, "cuEventRecord"},
    {0, "cuEventDestroy"},
    {0, "cuMemcpyHtoDAsync_v2"},
    {0, "cuMemcpyHtoD_v2"},
    {0, "cuMemcpyDtoHAsync_v2"},
    {0, "cuMemcpyDtoH_v2"},
    {0, "cuEventElapsedTime"}
};
*/

stream_to_id_map_t stream_to_id[MAX_STREAMS];

int g_cpu_gpu_proxy_count = 0; 
bool g_cpu_gpu_enabled = false;

int cpu_idle_metric_id;
int gpu_activity_time_metric_id;
int cpu_idle_cause_metric_id;
int gpu_idle_metric_id;
int cpu_overlap_metric_id;
int gpu_overlap_metric_id;
int stream_special_metric_id;
int h_to_d_data_xfer_metric_id;
int d_to_h_data_xfer_metric_id;


uint64_t g_active_threads;
stream_node_t *g_unfinished_stream_list_head;
event_list_node_t *g_finished_event_nodes_tail;
event_list_node_t dummy_event_node = {.event_end = 0,.event_start = 0,.event_end_time = 0,.event_end_time = 0,.launcher_cct = 0,.stream_launcher_cct = 0 };
struct stream_to_id_map_t *stream_to_id_tree_root = NULL;


stream_node_t g_stream_array[MAX_STREAMS];
spinlock_t g_gpu_lock = SPINLOCK_UNLOCKED;
bool g_do_shared_blaming = false;

// ******* METHOD DEFINITIONS ***********




static void PopulateEntryPointesToWrappedCudaRuntimeCalls() {
    void *handle;
    char *error;
    
    handle = dlopen("libcudart.so", RTLD_LAZY);
    if (!handle) {
        fputs(dlerror(), stderr);
        exit(1);
    }
    
    for (int i = 0; i < CUDA_MAX_APIS; i++) {
        cudaRuntimeFunctionPointer[i].generic = dlsym(handle, cudaRuntimeFunctionPointer[i].functionName);
        if ((error = dlerror()) != NULL) {
            fprintf(stderr, "%s\n", error);
            exit(1);
        }
    }
}

static void PopulateEntryPointesToWrappedCuDriverCalls() {
    void *handle;
    char *error;
    
    handle = dlopen("libcuda.so", RTLD_LAZY);
    if (!handle) {
        fputs(dlerror(), stderr);
        exit(1);
    }
    
    for (int i = 0; i < CU_MAX_APIS; i++) {
        cuDriverFunctionPointer[i].generic = dlsym(handle, cuDriverFunctionPointer[i].functionName);
        if ((error = dlerror()) != NULL) {
            fprintf(stderr, "%s\n", error);
            exit(1);
        }
    }
}

static void PopulateEntryPointesToWrappedCalls() {
    PopulateEntryPointesToWrappedCudaRuntimeCalls();
    PopulateEntryPointesToWrappedCuDriverCalls();
}


void CloseAllStreams(stream_to_id_map_t *root) {
    
    if (!root)
        return;
    
    CloseAllStreams(root->left);
    CloseAllStreams(root->right);
    uint32_t streamId;
    streamId = root->id;
    
    gpu_trace_close(g_stream_array[streamId].st, DEVICE_ID, streamId);
    hpcrun_stream_finalize(g_stream_array[streamId].st);
    
    //gpu_trace_close(DEVICE_ID, streamId);
    //hpcrun_stream_finalize(g_stream_array[streamId].st);
    g_stream_array[streamId].st = NULL;
}


static void METHOD_FN(init)
{
    char * shared_blaming_env;    
    TMSG(CPU_GPU_BLAME_CTL, "setting up CPU_GPU_BLAME");
    g_unfinished_stream_list_head = NULL;
    g_finished_event_nodes_tail = &dummy_event_node;
    dummy_event_node.next = g_finished_event_nodes_tail;
    PopulateEntryPointesToWrappedCalls();
    shared_blaming_env = getenv("HPCRUN_ENABLE_SHARED_GPU_BLAMING");
    if(shared_blaming_env)
        g_do_shared_blaming = atoi(shared_blaming_env);
    
    self->state = INIT;                                    
}

static void METHOD_FN(thread_init)
{
    TMSG(CPU_GPU_BLAME_CTL, "thread init");
    atomic_add_i64(&g_active_threads, 1L);
}

static void METHOD_FN(thread_init_action)
{
    TMSG(CPU_GPU_BLAME_CTL, "thread action (noop)");
}

static void
METHOD_FN(start)
{
    TMSG(CPU_GPU_BLAME_CTL,"starting CPU_GPU_BLAME");
    if (!g_cpu_gpu_proxy_count) {
        EMSG("Either pass -e WALLCLOKC or -e PAPI_TOT_CYC to enable CPU_GPU_BLAME");
        monitor_real_abort();
    }
    g_cpu_gpu_enabled = true;
    TD_GET(ss_state)[self->evset_idx] = START;
}





static void METHOD_FN(thread_fini_action)
{
    TMSG(CPU_GPU_BLAME_CTL, "thread action ");
    
#if 1
    if (fetch_and_add(&g_active_threads, -1L) == 1L) {
        /*FIXME: REVISIT to see how this interacts with the finalize in hpcrun
         * right now, this causes a hang.
         */
        SYNCHRONOUS_CLEANUP;
        // Walk the stream splay tree and close each trace.
        CloseAllStreams(stream_to_id_tree_root);
        stream_to_id_tree_root = NULL;
    }
#endif
}

static void METHOD_FN(stop)
{
    TMSG(CPU_GPU_BLAME_CTL, "stopping CPU_GPU_BLAME");
    TD_GET(ss_state)[self->evset_idx] = STOP;
}

static void METHOD_FN(shutdown)
{
    
    
#if 0
    if (!g_stream0_not_initialized) {
        fprintf(stderr, "\n MAY BE BROKEN FOR MULTITHREADED");
        uint32_t streamId;
        streamId = SplayGetStreamId(stream_to_id_tree_root, 0);
        SYNCHRONOUS_CLEANUP;
        gpu_trace_close(DEVICE_ID, streamId);
    }
#endif
    
    TMSG(CPU_GPU_BLAME_CTL, "shutodown CPU_GPU_BLAME_CTL");
    METHOD_CALL(self, stop);    // make sure stop has been called
    
    self->state = UNINIT;
}

static bool METHOD_FN(supports_event, const char *ev_str)
{    
    return ((strstr(ev_str, "CPU_GPU_IDLE") != NULL));
}



static void METHOD_FN(process_event_list, int lush_metrics)
{
    
    TMSG(CPU_GPU_BLAME_CTL, "process event list, lush_metrics = %d", lush_metrics);

    // Create metrics for CPU/GPU blame shifting
    // cpu_idle_metric_id a.k.a CPU_IDLE measures the time when CPU is idle waiting for GPU to finish 
    cpu_idle_metric_id = hpcrun_new_metric();
    // cpu_idle_cause_metric_id a.k.a CPU_IDLE_CAUSE blames GPU kernels (CCT nodes which launched them) 
    // that are keeping the CPU  idle 
    cpu_idle_cause_metric_id = hpcrun_new_metric();
    // gpu_idle_metric_id a.k.a GPU_IDLE_CAUSE measures the time when GPU is idle and blames CPU CCT node
    // for not creating work
    gpu_idle_metric_id = hpcrun_new_metric();
    // cpu_overlap_metric_id a.k.a OVERLAPPED_CPU attributes the time to CPU CCT node  concurrently
    // executing with GPU 
    cpu_overlap_metric_id = hpcrun_new_metric();
    // gpu_overlap_metric_id a.k.a OVERLAPPED_GPU attributes the time to GPU kernel (CCT node which launched it)
    // concurrently executing with CPU 
    gpu_overlap_metric_id = hpcrun_new_metric();
    
    // gpu_activity_time_metric_id a.k.a. GPU_ACTIVITY_TIME accounts the absolute running time of a kernel (CCT node which launched it)
    gpu_activity_time_metric_id = hpcrun_new_metric();
    
    
    // h_to_d_data_xfer_metric_id is the number of bytes xfered from CPU to GPU
    h_to_d_data_xfer_metric_id = hpcrun_new_metric();
    
    // d_to_h_data_xfer_metric_id is the number of bytes xfered from GPU to CPU
    d_to_h_data_xfer_metric_id = hpcrun_new_metric();
    
    
    hpcrun_set_metric_info_and_period(cpu_idle_metric_id, "CPU_IDLE", MetricFlags_ValFmt_Int, 1);
    hpcrun_set_metric_info_and_period(gpu_idle_metric_id, "GPU_IDLE_CAUSE", MetricFlags_ValFmt_Int, 1);
    hpcrun_set_metric_info_and_period(cpu_idle_cause_metric_id, "CPU_IDLE_CAUSE", MetricFlags_ValFmt_Real, 1);
    hpcrun_set_metric_info_and_period(cpu_overlap_metric_id, "OVERLAPPED_CPU", MetricFlags_ValFmt_Real, 1);
    hpcrun_set_metric_info_and_period(gpu_overlap_metric_id, "OVERLAPPED_GPU", MetricFlags_ValFmt_Real, 1);
    hpcrun_set_metric_info_and_period(gpu_activity_time_metric_id, "GPU_ACTIVITY_TIME", MetricFlags_ValFmt_Int, 1);
    
    hpcrun_set_metric_info_and_period(h_to_d_data_xfer_metric_id, "H_TO_D_BYTES", MetricFlags_ValFmt_Int, 1);
    hpcrun_set_metric_info_and_period(d_to_h_data_xfer_metric_id, "D_TO_H_BYTES", MetricFlags_ValFmt_Int, 1);

    // 
    thread_data_t *td = hpcrun_get_thread_data();
    td->eventSet[self->evset_idx] = 0xDEAD;
}

static void METHOD_FN(gen_event_set, int lush_metrics)
{
    // There is NO signal hander for us, we proxy with itimer or PAPI_TOT_CYC
}

static void METHOD_FN(display_events)
{
    printf("===========================================================================\n");
    printf("Available CPU_GPU_IDLE events\n");
    printf("===========================================================================\n");
    printf("Name\t\tDescription\n");
    printf("---------------------------------------------------------------------------\n");
    printf("CPU_GPU_IDLE\tCPU GPU idleness\n");
    printf("\n");
}

/***************************************************************************
 * object
 ***************************************************************************/

#define ss_name cpu_gpu_idle
#define ss_cls SS_HARDWARE

#include "ss_obj.h"

/******************************************************************************
 * private operations 
 *****************************************************************************/

