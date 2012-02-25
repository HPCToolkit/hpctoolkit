// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
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
// ******************************************************* EndRiceCopyright *

//
// itimer sample source simple oo interface
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
 * GPU support
 *****************************************************************************/
#define ENABLE_CUDA

#ifdef ENABLE_CUDA
#include <cuda.h>
#include <cupti.h>
#include <dlfcn.h>
#endif

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

/******************************************************************************
 * macros
 *****************************************************************************/

#if defined(CATAMOUNT)
#   define HPCRUN_PROFILE_SIGNAL           SIGALRM
#   define HPCRUN_PROFILE_TIMER            ITIMER_REAL
#else
#  define HPCRUN_PROFILE_SIGNAL            SIGPROF
#  define HPCRUN_PROFILE_TIMER             ITIMER_PROF
#endif

#define SECONDS_PER_HOUR                   3600

#if !defined(HOST_SYSTEM_IBM_BLUEGENE)
#  define USE_ELAPSED_TIME_FOR_WALLCLOCK
#endif

#define RESET_ITIMER_EACH_SAMPLE

#if defined(RESET_ITIMER_EACH_SAMPLE)

#  if defined(HOST_SYSTEM_IBM_BLUEGENE)
  //--------------------------------------------------------------------------
  // Blue Gene/P compute node support for itimer incorrectly delivers SIGALRM
  // in one-shot mode. To sidestep this problem, we use itimer in 
  // interval mode, but with an interval so long that we never expect to get 
  // a repeat interrupt before resetting it. 
  //--------------------------------------------------------------------------
#    define AUTOMATIC_ITIMER_RESET_SECONDS(x)            (SECONDS_PER_HOUR)
#    define AUTOMATIC_ITIMER_RESET_MICROSECONDS(x)       (0)
#  else                         // !defined(HOST_SYSTEM_IBM_BLUEGENE)
#    define AUTOMATIC_ITIMER_RESET_SECONDS(x)            (0)
#    define AUTOMATIC_ITIMER_RESET_MICROSECONDS(x)       (0)
#  endif                        // !defined(HOST_SYSTEM_IBM_BLUEGENE)

#else                           // !defined(RESET_ITIMER_EACH_SAMPLE)

#  define AUTOMATIC_ITIMER_RESET_SECONDS(x)              (x)
#  define AUTOMATIC_ITIMER_RESET_MICROSECONDS(x)         (x)

#endif                          // !defined(RESET_ITIMER_EACH_SAMPLE)

#define DEFAULT_PERIOD  5000L

#ifdef ENABLE_CUDA

   // MACROS for error checking CUDA/CUPTI APIs

#define CHECK_CU_ERROR(err, cufunc)                                     \
  if (err != CUDA_SUCCESS)                                              \
    {                                                                   \
      printf ("%s:%d: error %d for CUDA Driver API function '%s'\n",    \
              __FILE__, __LINE__, err, cufunc);                         \
      exit(-1);                                                         \
    }

#define CHECK_CUPTI_ERROR(err, cuptifunc)                               \
  if (err != CUPTI_SUCCESS)                                             \
    {                                                                   \
      const char *errstr;                                               \
      cuptiGetResultString(err, &errstr);                               \
      printf ("%s:%d:Error %s for CUPTI API function '%s'.\n",          \
              __FILE__, __LINE__, errstr, cuptifunc);                   \
      exit(-1);                                                         \
    }


#define MAX_STREAMS 100
#define NUM_GPU_DEVICES 2
#define GET_STREAM_ID(x) ((x) - g_stream_array)

#define fprintf(...) do{}while(0)
#define fflush(...) do{}while(0)

#define ADD_TO_FREE_EVENTS_LIST(node_ptr) do { (node_ptr)->next_free_node = g_free_event_nodes_head; \
				g_free_event_nodes_head = (node_ptr); }while(0)

#endif                          //ENABLE_CUDA



/******************************************************************************
 * local constants
 *****************************************************************************/

enum _local_const {
    ITIMER_EVENT = 0            // itimer has only 1 event
};

/******************************************************************************
 * forward declarations 
 *****************************************************************************/

static int itimer_signal_handler(int sig, siginfo_t * siginfo, void *context);

#ifdef ENABLE_CUDA

typedef struct event_list_node {
    cudaEvent_t event;
    cudaEvent_t event_start;
    cct_node_t *launcher_cct;
    struct event_list_node *next;
    struct event_list_node *next_free_node;
} event_list_node;

/*
 *   array of g_stream_array
 *   this is for GPU purpose only
 */
typedef struct stream_node {
    cudaEvent_t stream_start_event;
    struct event_list_node *latest_event_node;
    struct event_list_node *unfinished_event_node;
    struct stream_node *next_unfinished_stream;
} stream_node;


void CUPTIAPI EventInsertionCallback(void *userdata, CUpti_CallbackDomain domain, CUpti_CallbackId cbid, const void *cbInfo);

#endif                          //ENABLE_CUDA
/******************************************************************************
 * local variables
 *****************************************************************************/

static struct itimerval itimer;

static const struct itimerval zerotimer = {
    .it_interval = {
                    .tv_sec = 0L,
                    .tv_usec = 0L},

    .it_value = {
                 .tv_sec = 0L,
                 .tv_usec = 0L}

};

static long period = DEFAULT_PERIOD;

static sigset_t sigset_itimer;

#ifdef ENABLE_CUDA
static spinlock_t g_gpu_lock = SPINLOCK_UNLOCKED;
static uint64_t g_num_threads_at_sync;

static stream_node *g_stream_array;
static stream_node *g_unfinished_stream_list_head;
static event_list_node *g_free_event_nodes_head;

#endif                          //ENABLE_CUDA


// ******* METHOD DEFINITIONS ***********

#ifdef ENABLE_CUDA


volatile int de = 1;
uint32_t cleanup_finished_events()
{
    uint32_t num_unfinished_streams = 0;
    stream_node *prev_stream = NULL;
    stream_node *next_stream = NULL;
    stream_node *cur_stream = g_unfinished_stream_list_head;
#ifdef DEBUG
    if (cur_stream) {
        fprintf(stderr, "\n Something to query in stream");
    }
#endif
    while (cur_stream != NULL) {
// DELETE ME
//while(de);
        assert(cur_stream->unfinished_event_node && " Can't point unfinished stream to null");
        next_stream = cur_stream->next_unfinished_stream;

        event_list_node *current_event = cur_stream->unfinished_event_node;
        while (current_event) {

            //cudaError_t err_cuda = cudaErrorNotReady;
            fprintf(stderr, "\n cudaEventQuery on  %p", current_event->event);
            //fflush(stdout);
            cudaError_t err_cuda = cudaEventQuery(current_event->event);
            if (err_cuda == cudaSuccess) {
                fprintf(stderr, "\n cudaEventQuery success %p", current_event->event);
                cct_node_t *launcher_cct = current_event->launcher_cct;
                hpcrun_cct_persistent_id_trace_mutate(launcher_cct);
                // record start time
                float elapsedTime; // in millisec with 0.5 microsec resolution as per CUDA
                err_cuda = cudaEventElapsedTime(&elapsedTime, cur_stream->stream_start_event, current_event->event_start);
                if (err_cuda != cudaSuccess)
                    fprintf(stderr, "\n cudaEventElapsedTime failed %p", current_event->event_start);
                uint64_t micro_time = elapsedTime * 1000;
#if 1
                fprintf(stderr, "\n started at  :  %lu", micro_time);
#endif
                // TODO : START TRACE WITH  with IDLE MARKER .. This should go away after trace viewer changes
                cct_bundle_t *bundle = &(TD_GET(epoch)->csdata);
                cct_node_t *idl = hpcrun_cct_bundle_get_idle_node(bundle);
                hpcrun_cct_persistent_id_trace_mutate(idl);
                gpu_trace_append_with_time(0, GET_STREAM_ID(cur_stream), hpcrun_cct_persistent_id(idl), micro_time - 1);

                gpu_trace_append_with_time( /*gpu_number */ 0, GET_STREAM_ID(cur_stream), hpcrun_cct_persistent_id(launcher_cct), micro_time);
                // record end time
                // TODO : WE JUST NEED TO PUT IDLE MARKER
                err_cuda = cudaEventElapsedTime(&elapsedTime, cur_stream->stream_start_event, current_event->event);
                if (err_cuda != cudaSuccess)
                    fprintf(stderr, "\n cudaEventElapsedTime failed %p", current_event->event);
                micro_time = elapsedTime * 1000;
#if 1
                fprintf(stderr, "\n Ended  at  :  %lu", micro_time);
#endif
                gpu_trace_append_with_time( /*gpu_number */ 0, GET_STREAM_ID(cur_stream), hpcrun_cct_persistent_id(launcher_cct), micro_time);


                // TODO : WE JUST NEED TO PUT IDLE MARKER
                gpu_trace_append_with_time( /*gpu_number */ 0, GET_STREAM_ID(cur_stream), hpcrun_cct_persistent_id(idl), micro_time + 1);
#if 1
                cudaEventElapsedTime(&elapsedTime, current_event->event_start, current_event->event);
                micro_time = elapsedTime * 1000;
                fprintf(stderr, "\n Length of Event :  %lu", micro_time);

#endif

                event_list_node *to_free = current_event;
                current_event = current_event->next;
                // Add to_free to fre list 
                ADD_TO_FREE_EVENTS_LIST(to_free);
            } else {
                fprintf(stderr, "\n cudaEventQuery failed %p", current_event->event);
                break;
            }
            //fflush(stdout);
        }

        cur_stream->unfinished_event_node = current_event;
        if (current_event == NULL) {
            // set oldest and newest pointers to null
            cur_stream->latest_event_node = NULL;
            if (prev_stream == NULL) {
                g_unfinished_stream_list_head = next_stream;
            } else {
                prev_stream->next_unfinished_stream = next_stream;
            }
        } else {

#if 0
            // Insert the unfinished activity into the trace ( we are sure we had a IDLE MARKER behind us)
            cct_node_t *launcher_cct = current_event->launcher_cct;
            hpcrun_cct_persistent_id_trace_mutate(launcher_cct);
            gpu_trace_append( /*gpu_number */ 0, GET_STREAM_ID(cur_stream), hpcrun_cct_persistent_id(launcher_cct));
#endif
            num_unfinished_streams++;
            prev_stream = cur_stream;
        }
        cur_stream = next_stream;
    }
#ifdef DEBUG
    fprintf(stderr, "\n UNDONE = %d", num_unfinished_streams);
#endif
    return num_unfinished_streams;
}


event_list_node *create_and_insert_event(int stream_id, cct_node_t * launcher_cct)
{
    event_list_node *event_node;
    if (g_free_event_nodes_head) {
        // get from free list
        event_node = g_free_event_nodes_head;
        g_free_event_nodes_head = g_free_event_nodes_head->next_free_node;
    } else {
        // allocate new node
        event_node = (event_list_node *) hpcrun_malloc(sizeof(event_list_node));
    }
    //cudaError_t err =  cudaEventCreateWithFlags(&(event_node->event),cudaEventDisableTiming);
    cudaError_t err = cudaEventCreate(&(event_node->event_start));
    CHECK_CU_ERROR(err, "cudaEventCreate");
    err = cudaEventCreate(&(event_node->event));
    CHECK_CU_ERROR(err, "cudaEventCreate");
    event_node->launcher_cct = launcher_cct;
    event_node->next = NULL;
    if (g_stream_array[stream_id].latest_event_node == NULL) {
        g_stream_array[stream_id].latest_event_node = event_node;
        g_stream_array[stream_id].unfinished_event_node = event_node;
        g_stream_array[stream_id].next_unfinished_stream = g_unfinished_stream_list_head;
        g_unfinished_stream_list_head = &(g_stream_array[stream_id]);
    } else {
        g_stream_array[stream_id].latest_event_node->next = event_node;
        g_stream_array[stream_id].latest_event_node = event_node;
    }
#if 0
    if (cudaEventQuery(event_node->event) == cudaSuccess) {
        fprintf(stderr, "\n  DONE AT ENTRY!!!");
    } else {
        fprintf(stderr, "\n NOT DONE AT ENTRY");
    }
#endif
    return event_node;
}




#endif                          // ENABLE_CUDA

static void METHOD_FN(init)
{
    TMSG(ITIMER_CTL, "setting up itimer interrupt");
    sigemptyset(&sigset_itimer);
    sigaddset(&sigset_itimer, HPCRUN_PROFILE_SIGNAL);

    int ret = monitor_real_sigprocmask(SIG_UNBLOCK, &sigset_itimer, NULL);

    if (ret) {
        EMSG("WARNING: Thread init could not unblock SIGPROF, ret = %d", ret);
    }
    self->state = INIT;


#ifdef ENABLE_CUDA
    // Initialize CUDA and CUPTI
    CUresult err;
    CUptiResult cuptiErr;
    int deviceCount;
    CUpti_SubscriberHandle subscriber;

    err = cuInit(0);
    // TODO: gracefully handle absense of CUDA
    CHECK_CU_ERROR(err, "cuInit");

    err = cuDeviceGetCount(&deviceCount);
    CHECK_CU_ERROR(err, "cuDeviceGetCount");

    // TODO: gracefully handle absense of device
    if (deviceCount == 0) {
        printf("There is no device supporting CUDA.\n");
        exit(-1);
    }
    // TODO: check device capabilities
    cuptiErr = cuptiSubscribe(&subscriber, (CUpti_CallbackFunc) EventInsertionCallback, 0);
    //TODO: gracefully handle failure
    CHECK_CUPTI_ERROR(cuptiErr, "cuptiSubscribe");

    // Enable runtime APIs
    // TODO: enable just the ones you need
    cuptiErr = cuptiEnableDomain(1 /*enable */ , subscriber, CUPTI_CB_DOMAIN_RUNTIME_API);
    CHECK_CUPTI_ERROR(cuptiErr, "cuptiEnableDomain");
    // Enable resource APIs
    cuptiErr = cuptiEnableDomain(1 /*enable */ , subscriber, CUPTI_CB_DOMAIN_RESOURCE);
    CHECK_CUPTI_ERROR(cuptiErr, "cuptiEnableDomain");
#if 0
    // Enable synchronization  APIs
    cuptiErr = cuptiEnableDomain(1 /*enable */ , subscriber, CUPTI_CB_DOMAIN_SYNCHRONIZE);
    CHECK_CUPTI_ERROR(cuptiErr, "cuptiEnableDomain");
#endif

    // Initialize CUDA events data structure 
    g_stream_array = (stream_node *) hpcrun_malloc(sizeof(stream_node) * MAX_STREAMS);
    memset(g_stream_array, 0, sizeof(stream_node) * MAX_STREAMS);
    g_unfinished_stream_list_head = NULL;

#endif                          // ENABLE_CUDA


}

static void METHOD_FN(thread_init)
{
    TMSG(ITIMER_CTL, "thread init (no action needed)");
}

static void METHOD_FN(thread_init_action)
{
    TMSG(ITIMER_CTL, "thread action (noop)");
}

static void METHOD_FN(start)
{
    if (!hpcrun_td_avail()) {
        TMSG(ITIMER_CTL, "Thread data unavailable ==> sampling suspended");
        return;                 // in the unlikely event that we are trying to start, but thread data is unavailable,
        // assume that all sample source ops are suspended.
    }

    TMSG(ITIMER_CTL, "starting itimer w value = (%d,%d), interval = (%d,%d)", itimer.it_value.tv_sec, itimer.it_value.tv_usec, itimer.it_interval.tv_sec, itimer.it_interval.tv_usec);

    if (setitimer(HPCRUN_PROFILE_TIMER, &itimer, NULL) != 0) {
        TMSG(ITIMER_CTL, "setitimer failed to start!!");
        EMSG("setitimer failed (%d): %s", errno, strerror(errno));
        hpcrun_ssfail_start("itimer");
    }
#ifdef USE_ELAPSED_TIME_FOR_WALLCLOCK
    int ret = time_getTimeReal(&TD_GET(last_time_us));
    if (ret != 0) {
        EMSG("time_getTimeReal (clock_gettime) failed!");
        abort();
    }
#endif

    TD_GET(ss_state)[self->evset_idx] = START;

}

static void METHOD_FN(thread_fini_action)
{
    TMSG(ITIMER_CTL, "thread action (noop)");
}

static void METHOD_FN(stop)
{
    int rc;

    rc = setitimer(HPCRUN_PROFILE_TIMER, &zerotimer, NULL);

    TMSG(ITIMER_CTL, "stopping itimer, ret = %d", rc);
    TD_GET(ss_state)[self->evset_idx] = STOP;
}

static void METHOD_FN(shutdown)
{
    TMSG(ITIMER_CTL, "shutodown itimer");
    METHOD_CALL(self, stop);    // make sure stop has been called

    int ret = monitor_real_sigprocmask(SIG_BLOCK, &sigset_itimer, NULL);
    if (ret) {
        EMSG("WARNING: process fini could not block SIGPROF, ret = %d", ret);
    }

    self->state = UNINIT;
}

static bool METHOD_FN(supports_event, const char *ev_str)
{
    return (strstr(ev_str, "WALLCLOCK") != NULL);
}

static int cpu_idle_metric_id;
static int cpu_idle_cause_metric_id;
static int gpu_idle_metric_id;
static int cpu_overlap_metric_id;
static int gpu_overlap_metric_id;

static void METHOD_FN(process_event_list, int lush_metrics)
{

    TMSG(ITIMER_CTL, "process event list, lush_metrics = %d", lush_metrics);
    // fetch the event string for the sample source
    char *_p = METHOD_CALL(self, get_event_str);

    //
    // EVENT: Only 1 wallclock event
    //
    char *event = start_tok(_p);

    char name[1024];            // local buffer needed for extract_ev_threshold

    TMSG(ITIMER_CTL, "checking event spec = %s", event);

    // extract event threshold
    hpcrun_extract_ev_thresh(event, sizeof(name), name, &period, DEFAULT_PERIOD);

    // store event threshold
    METHOD_CALL(self, store_event, ITIMER_EVENT, period);
    TMSG(OPTIONS, "wallclock period set to %ld", period);

    // set up file local variables for sample source control
    int seconds = period / 1000000;
    int microseconds = period % 1000000;

    TMSG(OPTIONS, "init timer w sample_period = %ld, seconds = %ld, usec = %ld", period, seconds, microseconds);

    // signal once after the given delay
    itimer.it_value.tv_sec = seconds;
    itimer.it_value.tv_usec = microseconds;

    // macros define whether automatic restart or not
    itimer.it_interval.tv_sec = AUTOMATIC_ITIMER_RESET_SECONDS(seconds);
    itimer.it_interval.tv_usec = AUTOMATIC_ITIMER_RESET_MICROSECONDS(microseconds);

    // handle metric allocation
    hpcrun_pre_allocate_metrics(1 + lush_metrics);

    int metric_id = hpcrun_new_metric();
    METHOD_CALL(self, store_metric_id, ITIMER_EVENT, metric_id);


    // set metric information in metric table

#ifdef USE_ELAPSED_TIME_FOR_WALLCLOCK
# define sample_period 1
#else
# define sample_period period
#endif

#ifdef ENABLE_CUDA

    // Create metrics for CPU/GPU blame shifting
    // cpu_idle_metric_id a.k.a CPU_IDLE measures the time when CPU is idle waiting for GPU to finish 
    cpu_idle_metric_id = hpcrun_new_metric();
    // cpu_idle_cause_metric_id a.k.a CPU_IDLE_CAUSE blames GPU kernels (CCT nodes which launched them) 
    // that are keeping the CPU  idle 
    cpu_idle_cause_metric_id = hpcrun_new_metric();
    // gpu_idle_metric_id a.k.a GPU_IDLE measures the time when GPU is idle and blames CPU CCT node
    // for not creating work
    gpu_idle_metric_id = hpcrun_new_metric();
    // cpu_overlap_metric_id a.k.a OVERLAPPED_CPU attributes the time to CPU CCT node  concurrently
    // executing with GPU 
    cpu_overlap_metric_id = hpcrun_new_metric();
    // gpu_overlap_metric_id a.k.a OVERLAPPED_GPU attributes the time to GPU kernel (CCT node which launched it)
    // concurrently executing with CPU 
    gpu_overlap_metric_id = hpcrun_new_metric();

    TMSG(ITIMER_CTL, "setting metric itimer period = %ld", sample_period);
    hpcrun_set_metric_info_and_period(metric_id, "WALLCLOCK (us)", MetricFlags_ValFmt_Int, sample_period);
    hpcrun_set_metric_info_and_period(cpu_idle_metric_id, "CPU_IDLE", MetricFlags_ValFmt_Real, 1);
    hpcrun_set_metric_info_and_period(gpu_idle_metric_id, "GPU_IDLE", MetricFlags_ValFmt_Real, 1);
    hpcrun_set_metric_info_and_period(cpu_idle_cause_metric_id, "CPU_IDLE_CAUSE", MetricFlags_ValFmt_Real, 1);
    hpcrun_set_metric_info_and_period(cpu_overlap_metric_id, "OVERLAPPED_CPU", MetricFlags_ValFmt_Real, 1);
    hpcrun_set_metric_info_and_period(gpu_overlap_metric_id, "OVERLAPPED_GPU", MetricFlags_ValFmt_Real, 1);
#endif                          // ENABLE_CUDA

    if (lush_metrics == 1) {
        int mid_idleness = hpcrun_new_metric();
        lush_agents->metric_time = metric_id;
        lush_agents->metric_idleness = mid_idleness;

        hpcrun_set_metric_info_and_period(mid_idleness, "idleness (us)", MetricFlags_ValFmt_Real, sample_period);
    }

    event = next_tok();
    if (more_tok()) {
        EMSG("MULTIPLE WALLCLOCK events detected! Using first event spec: %s");
    }
    // 
    thread_data_t *td = hpcrun_get_thread_data();
    td->eventSet[self->evset_idx] = 0xDEAD;
}

//
// There is only 1 event for itimer, hence the event "set" is always the same.
// The signal setup, however, is done here.
//
static void METHOD_FN(gen_event_set, int lush_metrics)
{
    monitor_sigaction(HPCRUN_PROFILE_SIGNAL, &itimer_signal_handler, 0, NULL);
}

#ifdef ENABLE_CUDA

cudaError_t cudaThreadSynchronize(void)
{
    spinlock_lock(&g_gpu_lock);
    static cudaError_t(*cudaThreadSynchronizeReal) (void) = NULL;
    void *handle;
    char *error;

    if (!cudaThreadSynchronizeReal) {
        handle = dlopen("libcudart.so", RTLD_LAZY);
        if (!handle) {
            fputs(dlerror(), stderr);
            exit(1);
        }
        cudaThreadSynchronizeReal = dlsym(handle, "cudaThreadSynchronize");
        if ((error = dlerror()) != NULL) {
            fprintf(stderr, "%s\n", error);
            exit(1);
        }
    }

    TD_GET(is_thread_at_cuda_sync) = true;
    spinlock_unlock(&g_gpu_lock);
    printf("\n calling cudaThreadSynchronize\n");
    cudaError_t ret = cudaThreadSynchronizeReal();
    printf("\n Done  cudaThreadSynchronize\n");
    TD_GET(is_thread_at_cuda_sync) = false;
    return ret;
}

cudaError_t cudaStreamSynchronize(cudaStream_t stream)
{
    spinlock_lock(&g_gpu_lock);
    static cudaError_t(*cudaStreamSynchronizeReal) (cudaStream_t) = NULL;
    void *handle;
    char *error;

    if (!cudaStreamSynchronizeReal) {
        handle = dlopen("libcudart.so", RTLD_LAZY);
        if (!handle) {
            fputs(dlerror(), stderr);
            exit(1);
        }
        cudaStreamSynchronizeReal = dlsym(handle, "cudaStreamSynchronize");
        if ((error = dlerror()) != NULL) {
            fprintf(stderr, "%s\n", error);
            exit(1);
        }
    }

    TD_GET(is_thread_at_cuda_sync) = true;
    spinlock_unlock(&g_gpu_lock);

    fprintf(stderr, "\n calling cudaStreamSynchronize\n");
    cudaError_t ret = cudaStreamSynchronizeReal(stream);
    fprintf(stderr, "\n Done  cudaStreamSynchronize\n");
    TD_GET(is_thread_at_cuda_sync) = false;
    return ret;
}


cudaError_t cudaEventSynchronize(cudaEvent_t event)
{
    spinlock_lock(&g_gpu_lock);

    static cudaError_t(*cudaEventSynchronizeReal) (cudaEvent_t) = NULL;
    void *handle;
    char *error;

    if (!cudaEventSynchronizeReal) {
        handle = dlopen("libcudart.so", RTLD_LAZY);
        if (!handle) {
            fputs(dlerror(), stderr);
            exit(1);
        }
        cudaEventSynchronizeReal = dlsym(handle, "cudaEventSynchronize");
        if ((error = dlerror()) != NULL) {
            fprintf(stderr, "%s\n", error);
            exit(1);
        }
    }

    TD_GET(is_thread_at_cuda_sync) = true;
    spinlock_unlock(&g_gpu_lock);

    fprintf(stderr, "\n calling cudaEventSynchronize\n");
    cudaError_t ret = cudaEventSynchronizeReal(event);
    fprintf(stderr, "\n Done  cudaEventSynchronize\n");
    TD_GET(is_thread_at_cuda_sync) = false;
    return ret;
}

cudaError_t cudaStreamWaitEvent(cudaStream_t stream, cudaEvent_t event, unsigned int flags)
{
    spinlock_lock(&g_gpu_lock);

    static cudaError_t(*cudaStreamWaitEventReal) (cudaStream_t, cudaEvent_t, unsigned int) = NULL;
    void *handle;
    char *error;

    if (!cudaStreamWaitEventReal) {
        handle = dlopen("libcudart.so", RTLD_LAZY);
        if (!handle) {
            fputs(dlerror(), stderr);
            exit(1);
        }
        cudaStreamWaitEventReal = dlsym(handle, "cudaStreamWaitEvent");
        if ((error = dlerror()) != NULL) {
            fprintf(stderr, "%s\n", error);
            exit(1);
        }
    }

    TD_GET(is_thread_at_cuda_sync) = true;
    spinlock_unlock(&g_gpu_lock);

    fprintf(stderr, "\n calling cudaStreamWaitEvent\n");
    cudaError_t ret = cudaStreamWaitEventReal(stream, event, flags);
    fprintf(stderr, "\n Done  cudaStreamWaitEvent\n");
    TD_GET(is_thread_at_cuda_sync) = false;
    return ret;
}




cudaError_t cudaDeviceSynchronize(void)
{
    spinlock_lock(&g_gpu_lock);

    static cudaError_t(*cudaDeviceSynchronizeReal) (void) = NULL;
    void *handle;
    char *error;

    if (!cudaDeviceSynchronizeReal) {
        handle = dlopen("libcudart.so", RTLD_LAZY);
        if (!handle) {
            fputs(dlerror(), stderr);
            exit(1);
        }
        cudaDeviceSynchronizeReal = dlsym(handle, "cudaDeviceSynchronize");
        if ((error = dlerror()) != NULL) {
            fprintf(stderr, "%s\n", error);
            exit(1);
        }
    }

    TD_GET(is_thread_at_cuda_sync) = true;
    spinlock_unlock(&g_gpu_lock);

    fprintf(stderr, "\n calling cudaDeviceSynchronize\n");
    cudaError_t ret = cudaDeviceSynchronizeReal();
    fprintf(stderr, "\n Done  cudaDeviceSynchronize\n");
    TD_GET(is_thread_at_cuda_sync) = false;
    return ret;
}

void CUPTIAPI EventInsertionCallback(void *userdata, CUpti_CallbackDomain domain, CUpti_CallbackId cbid, const void *cbData)
{

    //CUptiResult cuptiErr;
    cudaError_t err;
    uint32_t streamId = 0;
    static cudaStream_t active_stream;
    static event_list_node *event_node;
    const CUpti_CallbackData *cbInfo;
    const CUpti_ResourceData *cbRDInfo;
    uint32_t new_streamId;


    switch (domain) {
    case CUPTI_CB_DOMAIN_RUNTIME_API:
        //TMSG(ITIMER_HANDLER,"call back CUPTI_CB_DOMAIN_RUNTIME_API");
        //printf("\ncall back CUPTI_CB_DOMAIN_RUNTIME_API");
        cbInfo = (const CUpti_CallbackData *) cbData;
        switch (cbid) {
        case CUPTI_RUNTIME_TRACE_CBID_cudaConfigureCall_v3020:{
                if (cbInfo->callbackSite == CUPTI_API_ENTER) {
                    // We cannot allow this thread to take samples since we will be holding a lock which is also needed in the async signal handler
                    hpcrun_async_block();
                    // let no other GPU work get ahead of me
                    spinlock_lock(&g_gpu_lock);
                    // Get stream id
                    cudaConfigureCall_v3020_params *params = (cudaConfigureCall_v3020_params *) cbInfo->functionParams;
                    active_stream = params->stream;
                }
            }
            break;

        case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyAsync_v3020:
            if (cbInfo->callbackSite == CUPTI_API_ENTER) {
                // We cannot allow this thread to take samples since we will be holding a lock which is also needed in the async signal handler
                hpcrun_async_block();
                // let no other GPU work get ahead of me
                spinlock_lock(&g_gpu_lock);
                //TODO: this should change, we should create streams for mem copies.
                cudaMemcpyAsync_v3020_params *params = (cudaMemcpyAsync_v3020_params *) cbInfo->functionParams;
                active_stream = params->stream;
                // Get CCT node (i.e., kernel launcher)
                ucontext_t context;
                getcontext(&context);
                //cct_node_t *node = hpcrun_gen_thread_ctxt(&context);
                cct_node_t *node = hpcrun_sample_callpath(&context, cpu_idle_metric_id, 1, 0 /*skipInner */ , 1 /*isSync */ );
                // Create a new Cuda Event
                cuptiGetStreamId(cbInfo->context, (CUstream) active_stream, &streamId);
                event_node = create_and_insert_event(streamId, node);
                // Insert the event in the stream
                err = cudaEventRecord(event_node->event_start, active_stream);
                CHECK_CU_ERROR(err, "cudaEventRecord");
            } else {
                err = cudaEventRecord(event_node->event, active_stream);
                CHECK_CU_ERROR(err, "cudaEventRecord");
                // let other things be queued into GPU
                spinlock_unlock(&g_gpu_lock);
                // safe to take async samples now
                hpcrun_async_unblock();
            }
            break;
            // case CUPTI_RUNTIME_TRACE_CBID_cudaMemsetAsync_v3020:
            //case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy_v3020:
            //case CUPTI_RUNTIME_TRACE_CBID_cudaMemset_v3020:
        case CUPTI_RUNTIME_TRACE_CBID_cudaLaunch_v3020:
            if (cbInfo->callbackSite == CUPTI_API_ENTER) {
                // Get CCT node (i.e., kernel launcher)
                ucontext_t context;
                getcontext(&context);
                //cct_node_t *node = hpcrun_gen_thread_ctxt(&context);
                cct_node_t *node = hpcrun_sample_callpath(&context, cpu_idle_metric_id, 1, 0 /*skipInner */ , 1 /*isSync */ );

                // Launcher CCT node will be 3 levels above in the loaded module ( Handler -> CUpti -> Cuda -> Launcher )
                node = hpcrun_get_cct_node_n_levels_up_in_load_module(node, 3);

                // Get stream Id
                cuptiGetStreamId(cbInfo->context, (CUstream) active_stream, &streamId);
                // Create a new Cuda Event
                cuptiGetStreamId(cbInfo->context, (CUstream) active_stream, &streamId);
                event_node = create_and_insert_event(streamId, node);
                // Insert the start event in the stream
                err = cudaEventRecord(event_node->event_start, active_stream);
                CHECK_CU_ERROR(err, "cudaEventRecord");
            } else {            // CUPTI_API_EXIT
                err = cudaEventRecord(event_node->event, active_stream);
                CHECK_CU_ERROR(err, "cudaEventRecord");
                // let other things be queued into GPU
                spinlock_unlock(&g_gpu_lock);
                // safe to take async samples now
                hpcrun_async_unblock();
            }
            break;
        default:
            break;
        }
        break;

    case CUPTI_CB_DOMAIN_RESOURCE:
        TMSG(ITIMER_HANDLER, "call back CUPTI_CB_DOMAIN_RESOURCE");
        printf("\ncall back CUPTI_CB_DOMAIN_RESOURCE");
        /*
         *FIXME: checkout what happens if a stream is being destroyed while another thread
         is providing work to it.
         */
        cbRDInfo = (const CUpti_ResourceData *) cbData;
        switch (cbid) {
        case CUPTI_CBID_RESOURCE_STREAM_CREATED:
            cuptiGetStreamId(cbRDInfo->context, cbRDInfo->resourceHandle.stream, &new_streamId);
            printf("\nStream is getting created: %d\n", new_streamId);
            // Initialize and Record an event to indicate the start of this stream.
            // No need to wait for it since we query only the events posted after this and this will be complete when the latter posted ones are complete.
            err = cudaEventCreate(&(g_stream_array[new_streamId].stream_start_event));
            CHECK_CU_ERROR(err, "cudaEventCreate");
            err = cudaEventRecord(g_stream_array[new_streamId].stream_start_event, cbRDInfo->resourceHandle.stream);
            CHECK_CU_ERROR(err, "cudaEventRecord");


            gpu_trace_open(0, new_streamId);
            cct_bundle_t *bundle = &(TD_GET(epoch)->csdata);
            cct_node_t *idl = hpcrun_cct_bundle_get_idle_node(bundle);
            hpcrun_cct_persistent_id_trace_mutate(idl);
            gpu_trace_append(0, new_streamId, hpcrun_cct_persistent_id(idl));


            break;
        case CUPTI_CBID_RESOURCE_STREAM_DESTROY_STARTING:
            cuptiGetStreamId(cbRDInfo->context, cbRDInfo->resourceHandle.stream, &new_streamId);
            printf("\nStream is getting destroyed: %d\n", new_streamId);
            gpu_trace_close(0, new_streamId);
            break;
        default:
            break;
        }
        break;
    default:
        printf("\ncall back DEF");
        break;
    }
}

#endif                          //ENABLE_CUDA

static void METHOD_FN(display_events)
{
    printf("===========================================================================\n");
    printf("Available itimer events\n");
    printf("===========================================================================\n");
    printf("Name\t\tDescription\n");
    printf("---------------------------------------------------------------------------\n");
    printf("WALLCLOCK\tWall clock time used by the process in microseconds\n");
    printf("\n");
}

/***************************************************************************
 * object
 ***************************************************************************/

#define ss_name itimer
#define ss_cls SS_HARDWARE

#include "ss_obj.h"

/******************************************************************************
 * private operations 
 *****************************************************************************/

static int itimer_signal_handler(int sig, siginfo_t * siginfo, void *context)
{
    // Must check for async block first and avoid any MSG if true.
    void *pc = hpcrun_context_pc(context);
    if (hpcrun_async_is_blocked(pc)) {
        if (ENABLED(ITIMER_CTL)) {
            ;                   // message to stderr here for debug
        }
        hpcrun_stats_num_samples_blocked_async_inc();
    } else {
        TMSG(ITIMER_HANDLER, "Itimer sample event");

        uint64_t metric_incr = 1; // default: one time unit
#ifdef USE_ELAPSED_TIME_FOR_WALLCLOCK
        uint64_t cur_time_us;
        int ret = time_getTimeReal(&cur_time_us);
        if (ret != 0) {
            EMSG("time_getTimeReal (clock_gettime) failed!");
            abort();
        }
        metric_incr = cur_time_us - TD_GET(last_time_us);
#endif

        int metric_id = hpcrun_event2metric(&_itimer_obj, ITIMER_EVENT);
        cct_node_t *node = hpcrun_sample_callpath(context, metric_id, metric_incr,
                                                  0 /*skipInner */ , 0 /*isSync */ );
///DELETE ME
        //fprintf(stderr,"\n Sample start");
#ifdef ENABLE_CUDA
        spinlock_lock(&g_gpu_lock);
        bool is_threads_at_sync = TD_GET(is_thread_at_cuda_sync);
        // when was the last cleanup done
        static uint64_t last_cleanup_time = 0;
        static uint32_t num_unfinshed_streams = 0;
        static stream_node *unfinished_event_list_head = 0;

        // if last cleanup happened (due to some other thread) less that metric_incr/2 time ago, then we will not do cleanup, but instead use its results
        if (cur_time_us - last_cleanup_time < metric_incr * 0.5) {
            // reuse the last time recorded num_unfinshed_streams and unfinished_event_list_head
        } else {
            //TODO: CUDA blocks thread from making cudaEventQuery() call if the thread is at SYNC
            // Hence we will not call cleanup if we know we are at SYNC
            if (is_threads_at_sync) {
                //fprintf(stderr,"\n Thread at sync");
                fflush(stderr);
                unfinished_event_list_head = g_unfinished_stream_list_head;
                num_unfinshed_streams = 0;
                for (stream_node * unfinished_stream = g_unfinished_stream_list_head; unfinished_stream; unfinished_stream = unfinished_stream->next_unfinished_stream)
                    num_unfinshed_streams++;

#ifdef DEBUG
                fprintf(stderr, ".%d", num_unfinshed_streams);
#endif
            } else {
                //fprintf(stderr,"\n Thread NOT at sync");
                //fprintf(stderr,"*");
                //fflush(stderr);
                // do cleanup 
                last_cleanup_time = cur_time_us;
                num_unfinshed_streams = cleanup_finished_events();
                unfinished_event_list_head = g_unfinished_stream_list_head;
            }
        }

        if (num_unfinshed_streams) {
            // GPU is busy if we are here

            if (is_threads_at_sync) {
                // CPU is idle

                // Increment CPU idleness by metric_incr
                cct_metric_data_increment(cpu_idle_metric_id, node, (cct_metric_data_t) {
                                          .r = metric_incr}
                );
                // Increment CPU idle cause by metric_incr/num_unfinshed_streams for each of unfinshed_streams
                for (stream_node * unfinished_stream = unfinished_event_list_head; unfinished_stream; unfinished_stream = unfinished_stream->next_unfinished_stream)
                    cct_metric_data_increment(cpu_idle_cause_metric_id, unfinished_stream->unfinished_event_node->launcher_cct, (cct_metric_data_t) {
                                              .r = metric_incr * 1.0 / num_unfinshed_streams}
                );
            } else {
                // CPU - GPU overlap

                // Increment cpu_overlap by metric_incr
                cct_metric_data_increment(cpu_overlap_metric_id, node, (cct_metric_data_t) {
                                          .r = metric_incr}
                );
                // Increment gpu_overlap by metric_incr/num_unfinshed_streams for each of the unfinshed streams
                for (stream_node * unfinished_stream = unfinished_event_list_head; unfinished_stream; unfinished_stream = unfinished_stream->next_unfinished_stream)
                    cct_metric_data_increment(gpu_overlap_metric_id, unfinished_stream->unfinished_event_node->launcher_cct, (cct_metric_data_t) {
                                              .r = metric_incr * 1.0 / num_unfinshed_streams}
                );

            }
        } else {
            // GPU is idle

            // Increment gpu_ilde by metric_incr
            cct_metric_data_increment(gpu_idle_metric_id, node, (cct_metric_data_t) {
                                      .r = metric_incr}
            );

        }
        spinlock_unlock(&g_gpu_lock);
#endif                          // ENABLE_CUDA
        //DELETE ME
        //fprintf(stderr,"\n Sample end");

    }
    if (hpcrun_is_sampling_disabled()) {
        TMSG(SPECIAL, "No itimer restart, due to disabled sampling");
        return 0;
    }
#ifdef RESET_ITIMER_EACH_SAMPLE
    METHOD_CALL(&_itimer_obj, start);
#endif

    return 0;                   /* tell monitor that the signal has been handled */
}
