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

#ifdef ENABLE_CUDA
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
#include <cuda.h>
#include <cuda_runtime.h>
//#include <cupti.h>
#include <dlfcn.h>
#include <ucontext.h>           /* struct ucontext */

/******************************************************************************
 * GPU support
 *****************************************************************************/
//#define CUDA_RT_API
#define DEVICE_ID 0

/******************************************************************************
 * libmonitor
 *****************************************************************************/

#include <monitor.h>

/******************************************************************************
 * local includes
 *****************************************************************************/
#include "sample_source_obj.h"
#include "common.h"

#include "stream.h"
//#include "stream_data.h"
//#include <hpcrun/write_stream_data.h>

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
#include <lib/prof-lean/splay-macros.h>

/******************************************************************************
 * macros
 *****************************************************************************/

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

#define CU_SAFE_CALL( call ) do {                                         \
CUresult err = call;                                                     \
if( CUDA_SUCCESS != err) {                                               \
fprintf(stderr, "Cuda driver error %d in call at file '%s' in line %i.\n",   \
err, __FILE__, __LINE__ );                                   \
exit(-1);                                                     \
} } while (0)

#define CUDA_SAFE_CALL( call) do {                                        \
cudaError_t err = call;                                                    \
if( cudaSuccess != err) {                                                \
fprintf(stderr, "Cuda error in call at file '%s' in line %i : %s.\n", \
__FILE__, __LINE__, cudaGetErrorString( err) );              \
exit(-1);                                                     \
} } while (0)

#define MAX_STREAMS 100
#define GET_STREAM_ID(x) ((x) - g_stream_array)
#define ALL_STREAMS_MASK (0xffffffff)
#define fprintf(...) do{}while(0)
#define fflush(...) do{}while(0)

#define ADD_TO_FREE_EVENTS_LIST(node_ptr) do { (node_ptr)->next_free_node = g_free_event_nodes_head; \
g_free_event_nodes_head = (node_ptr); }while(0)

#define ADD_TO_FREE_TREE_NODE_LIST(node_ptr) do { (node_ptr)->next_free_node = g_free_tree_nodes_head; \
g_free_tree_nodes_head = (node_ptr); }while(0)

#define ADD_TO_FREE_ACTIVE_KERNEL_NODE_LIST(node_ptr) do { (node_ptr)->next_free_node = g_free_active_kernel_nodes_head; \
g_free_active_kernel_nodes_head = (node_ptr); }while(0)

#define HPCRUN_ASYNC_BLOCK_SPIN_LOCK  do{hpcrun_async_block(); \
spinlock_lock(&g_gpu_lock);} while(0)

#define HPCRUN_ASYNC_UNBLOCK_SPIN_UNLOCK  do{spinlock_unlock(&g_gpu_lock); \
hpcrun_async_unblock();} while(0)

#define OLD_SYNC_PROLOGUE(ctxt, launch_node, start_time, rec_node) \
hpcrun_async_block();      \
ucontext_t ctxt;           \
getcontext(&ctxt);         \
cct_node_t * launch_node = hpcrun_sample_callpath(&ctxt, cpu_idle_metric_id, 0 , 0 /*skipInner */ , 1 /*isSync */ ); \
hpcrun_cct_persistent_id_trace_mutate(launch_node);     \
trace_append(hpcrun_cct_persistent_id(launch_node));    \
TD_GET(is_thread_at_cuda_sync) = true;                      \
spinlock_lock(&g_gpu_lock);                                 \
uint64_t start_time;                                         \
event_list_node  *  rec_node = EnterCudaSync(& start_time); \
spinlock_unlock(&g_gpu_lock)

#define OLD_SYNC_EPILOGUE(ctxt, launch_node, start_time, rec_node, mask, end_time)      \
spinlock_lock(&g_gpu_lock);                                     \
LeaveCudaSync(rec_node,start_time,mask);                        \
spinlock_unlock(&g_gpu_lock);                                   \
struct timeval tv;                                              \
gettimeofday(&tv, NULL);                                        \
uint64_t end_time  = ((uint64_t)tv.tv_usec + (((uint64_t)tv.tv_sec) * 1000000)); \
cct_metric_data_increment(cpu_idle_metric_id, launch_node, (cct_metric_data_t) {.i = (end_time - start_time)});  \
cct_metric_data_increment(wall_clock_metric_id, launch_node, (cct_metric_data_t) {.i = (end_time - start_time)});    \
trace_append(hpcrun_cct_persistent_id(launch_node));            \
hpcrun_async_unblock();                                         \
TD_GET(is_thread_at_cuda_sync) = false

#define SYNC_PROLOGUE(ctxt, launch_node, start_time, rec_node) \
hpcrun_async_block();      \
ucontext_t ctxt;           \
getcontext(&ctxt);         \
cct_node_t * launch_node = hpcrun_sample_callpath(&ctxt, cpu_idle_metric_id, 0 , 0 /*skipInner */ , 1 /*isSync */ ); \
TD_GET(is_thread_at_cuda_sync) = true;                      \
spinlock_lock(&g_gpu_lock);                                 \
uint64_t start_time;                                        \
event_list_node  *  rec_node = EnterCudaSync(& start_time); \
spinlock_unlock(&g_gpu_lock);                               \
hpcrun_async_unblock();

#define SYNC_EPILOGUE(ctxt, launch_node, start_time, rec_node, mask, end_time)      \
hpcrun_async_block();\
spinlock_lock(&g_gpu_lock);\
uint64_t last_kernel_end_time = LeaveCudaSync(rec_node,start_time,mask);\
spinlock_unlock(&g_gpu_lock);\
struct timeval tv;\
gettimeofday(&tv, NULL);\
uint64_t end_time  = ((uint64_t)tv.tv_usec + (((uint64_t)tv.tv_sec) * 1000000)); \
if ( last_kernel_end_time > end_time) {last_kernel_end_time = end_time;}\
uint64_t cpu_idle_time = last_kernel_end_time == 0 ? 0: last_kernel_end_time  - start_time;\
uint64_t gpu_idle_time = last_kernel_end_time == 0 ? end_time - start_time : end_time - last_kernel_end_time;\
cct_metric_data_increment(cpu_idle_metric_id, launch_node, (cct_metric_data_t) {.i = (cpu_idle_time)});\
cct_metric_data_increment(gpu_idle_metric_id, launch_node, (cct_metric_data_t) {.i = (gpu_idle_time)});\
hpcrun_async_unblock();\
TD_GET(is_thread_at_cuda_sync) = false

#define GET_NEW_TREE_NODE(node_ptr) do {						\
if (g_free_tree_nodes_head) {							\
node_ptr = g_free_tree_nodes_head;					\
g_free_tree_nodes_head = g_free_tree_nodes_head->next_free_node;	\
} else {									\
node_ptr = (tree_node *) hpcrun_malloc(sizeof(tree_node));		\
}										\
} while(0)

#define GET_NEW_ACTIVE_KERNEL_NODE(node_ptr) do {							\
if (g_free_active_kernel_nodes_head) {								\
node_ptr = g_free_active_kernel_nodes_head;						\
g_free_active_kernel_nodes_head = g_free_active_kernel_nodes_head->next_free_node;	\
} else {											\
node_ptr = (active_kernel_node *) hpcrun_malloc(sizeof(active_kernel_node));		\
}												\
} while(0)

///TODO: evaluate this option : FORCE CLEANUP
#define SYNCHRONOUS_CLEANUP do{  hpcrun_async_block(); 		\
spinlock_lock(&g_gpu_lock); 			\
cleanup_finished_events(); 			\
spinlock_unlock(&g_gpu_lock);			\
hpcrun_async_unblock(); } while(0)

/******************************************************************************
 * local constants
 *****************************************************************************/

enum _cuda_const {
    KERNEL_START,
    KERNEL_END
};
/******************************************************************************
 * forward declarations 
 *****************************************************************************/

typedef struct event_list_node {

#ifdef CUDA_RT_API
    cudaEvent_t event_end;
    cudaEvent_t event_start;
#else                           // Driver API
    CUevent event_end;
    CUevent event_start;
#endif

    uint64_t event_start_time;
    uint64_t event_end_time;
    cct_node_t *launcher_cct;
    cct_node_t *stream_launcher_cct;
    uint32_t ref_count;
    uint32_t stream_id;
    union {
        struct event_list_node *next;
        struct event_list_node *next_free_node;
    };
} event_list_node;

typedef struct active_kernel_node {
    uint64_t event_time;
    bool event_type;
    uint32_t stream_id;
    union {
        cct_node_t *launcher_cct;       // present only in START nodes
        struct active_kernel_node *start_node;
    };
    union {
        struct active_kernel_node *next;
        struct active_kernel_node *next_free_node;
    };
    struct active_kernel_node *next_active_kernel;
    struct active_kernel_node *prev;

} active_kernel_node;

/*
 *   array of g_stream_array
 *   this is for GPU purpose only
 */
typedef struct stream_node {
    struct stream_data_t *st;
    struct event_list_node *latest_event_node;
    struct event_list_node *unfinished_event_node;
    struct stream_node *next_unfinished_stream;
} stream_node;

typedef struct stream_to_id_map {
#ifdef CUDA_RT_API
    cudaStream_t stream;
#else                           // Driver API
    CUstream stream;
#endif
    uint32_t id;
    struct stream_to_id_map *left;
    struct stream_to_id_map *right;
} stream_to_id_map;

stream_to_id_map stream_to_id[MAX_STREAMS];

#ifdef CUDA_RT_API

enum cudaRuntimeAPIIndex {
    CUDA_THREAD_SYNCHRONIZE,
    CUDA_STREAM_SYNCHRONIZE,
    CUDA_EVENT_SYNCHRONIZE,
    CUDA_STREAM_WAIT_EVENT,
    CUDA_DEVICE_SYNCHRONIZE,
    CUDA_CONFIGURE_CALL,
    CUDA_LAUNCH,
    CUDA_STREAM_DESTROY,
    CUDA_STREAM_CREATE,
    CUDA_MALLOC,
    CUDA_FREE,
    CUDA_MEMCPY_ASYNC,
    CUDA_MEMCPY,
    CUDA_MEMCPY_2D,
    CUDA_EVENT_ELAPSED_TIME,
    CUDA_EVENT_CREATE,
    CUDA_EVENT_RECORD,

    CUDA_MAX_APIS
};

typedef struct cudaRuntimeFunctionPointer {
    union {
        cudaError_t(*generic) (void);
        cudaError_t(*cudaThreadSynchronizeReal) (void);
        cudaError_t(*cudaStreamSynchronizeReal) (cudaStream_t);
        cudaError_t(*cudaEventSynchronizeReal) (cudaEvent_t);
        cudaError_t(*cudaStreamWaitEventReal) (cudaStream_t, cudaEvent_t, unsigned int);
        cudaError_t(*cudaDeviceSynchronizeReal) (void);
        cudaError_t(*cudaConfigureCallReal) (dim3, dim3, size_t, cudaStream_t);
        cudaError_t(*cudaLaunchReal) (const char *);
        cudaError_t(*cudaStreamDestroyReal) (cudaStream_t);
        cudaError_t(*cudaStreamCreateReal) (cudaStream_t *);
        cudaError_t(*cudaMallocReal) (void **, size_t);
        cudaError_t(*cudaFreeReal) (void *);
        cudaError_t(*cudaMemcpyAsyncReal) (void *dst, const void *src, size_t count, enum cudaMemcpyKind kind, cudaStream_t);
        cudaError_t(*cudaMemcpyReal) (void *dst, const void *src, size_t count, enum cudaMemcpyKind kind);
        cudaError_t(*cudaMemcpy2DReal) (void *dst, size_t dpitch, const void *src, size_t spitch, size_t width, size_t height, enum cudaMemcpyKind kind);
        cudaError_t(*cudaEventElapsedTimeReal) (float *ms, cudaEvent_t start, cudaEvent_t end);
        cudaError_t(*cudaEventCreateReal) (cudaEvent_t * event);
        cudaError_t(*cudaEventRecordReal) (cudaEvent_t event, cudaStream_t stream);

    };
    const char *functionName;
} cudaRuntimeFunctionPointer_t;

cudaRuntimeFunctionPointer_t cudaRuntimeFunctionPointer[] = {
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
};

#else                           // Driver API
enum cuDriverAPIIndex {
    CU_STREAM_CREATE,
    CU_STREAM_DESTROY,
    CU_STREAM_SYNCHRONIZE,
    CU_EVENT_SYNCHRONIZE,
    CU_LAUNCH_GRID_ASYNC,
    CU_CTX_DESTROY,

    CU_EVENT_CREATE,
    CU_EVENT_RECORD,

    CU_MEMCPY_H_TO_D_ASYNC,
    CU_MEMCPY_H_TO_D,
    CU_MEMCPY_D_TO_H_ASYNC,
    CU_MEMCPY_D_TO_H,

    CU_EVENT_ELAPSED_TIME,

    CU_MAX_APIS
};

typedef struct cuDriverFunctionPointer {
    union {
        CUresult(*generic) (void);
        CUresult(*cuStreamCreateReal) (CUstream phStream, unsigned int Flags);
        CUresult(*cuStreamDestroyReal) (CUstream hStream);
        CUresult(*cuStreamSynchronizeReal) (CUstream hStream);
        CUresult(*cuEventSynchronizeReal) (CUevent event);
        CUresult(*cuLaunchGridAsyncReal) (CUfunction f, int grid_width, int grid_height, CUstream hStream);
        CUresult(*cuCtxDestroyReal) (CUcontext ctx);

        CUresult(*cuEventCreateReal) (CUevent * phEvent, unsigned int Flags);
        CUresult(*cuEventRecordReal) (CUevent hEvent, CUstream hStream);

        CUresult(*cuMemcpyHtoDAsyncReal) (CUdeviceptr dstDevice, const void *srcHost, size_t ByteCount, CUstream hStream);
        CUresult(*cuMemcpyHtoDReal) (CUdeviceptr dstDevice, const void *srcHost, size_t ByteCount);
        CUresult(*cuMemcpyDtoHAsyncReal) (void *dstHost, CUdeviceptr srcDevice, size_t ByteCount, CUstream hStream);
        CUresult(*cuMemcpyDtoHReal) (void *dstHost, CUdeviceptr srcDevice, size_t ByteCount);

        CUresult(*cuEventElapsedTimeReal) (float *pMilliseconds, CUevent hStart, CUevent hEnd);

    };
    const char *functionName;
} cuDriverFunctionPointer_t;

cuDriverFunctionPointer_t cuDriverFunctionPointer[] = {
    {0, "cuStreamCreate"},
    {0, "cuStreamDestroy"},
    {0, "cuStreamSynchronize"},
    {0, "cuEventSynchronize"},
    {0, "cuLaunchGridAsync"},
    {0, "cuCtxDestroy"},
    {0, "cuEventCreate"},
    {0, "cuEventRecord"},
    {0, "cuMemcpyHtoDAsync_v2"},
    {0, "cuMemcpyHtoD_v2"},
    {0, "cuMemcpyDtoHAsync_v2"},
    {0, "cuMemcpyDtoH_v2"},
    {0, "cuEventElapsedTime"}
};

#endif

static uint32_t cleanup_finished_events();
//void CUPTIAPI EventInsertionCallback(void *userdata, CUpti_CallbackDomain domain, CUpti_CallbackId cbid, const void *cbInfo);

/******************************************************************************
 * local variables
 *****************************************************************************/

static spinlock_t g_gpu_lock = SPINLOCK_UNLOCKED;
//Lock for wrapped synchronous calls
static spinlock_t g_cuda_lock = SPINLOCK_UNLOCKED;
static struct stream_to_id_map *stream_to_id_tree_root = NULL;
// TODO.. Karthiks CCT fix needed, 32 since I assume max of 32 CPU threads
static uint32_t g_stream_id = 32;
static uint32_t stream_to_id_index = 0;

// Lock for stream to id map
static spinlock_t g_stream_id_lock = SPINLOCK_UNLOCKED;

static uint64_t g_num_threads_at_sync;
static uint64_t g_active_threads;
static stream_node g_stream_array[MAX_STREAMS];
static stream_node *g_unfinished_stream_list_head;

static event_list_node *g_free_event_nodes_head;
static event_list_node *g_finished_event_nodes_tail;
static active_kernel_node *g_free_active_kernel_nodes_head;
static event_list_node dummy_event_node = {.event_end = 0,.event_start = 0,.event_end_time = 0,.event_end_time = 0,.launcher_cct = 0,.stream_launcher_cct = 0 };

static int wall_clock_metric_id;
static int cpu_idle_metric_id;
static int gpu_activity_time_metric_id;
static int cpu_idle_cause_metric_id;
static int gpu_idle_metric_id;
static int cpu_overlap_metric_id;
static int gpu_overlap_metric_id;
static int stream_special_metric_id;

static uint64_t g_start_of_world_time;

#ifdef CUDA_RT_API
static cudaEvent_t g_start_of_world_event;
#else                           // Driver API
static CUevent g_start_of_world_event;
#endif

static bool g_stream0_not_initialized = true;

// ******* METHOD DEFINITIONS ***********

#ifdef CUDA_RT_API
static struct stream_to_id_map *splay(struct stream_to_id_map *root, cudaStream_t key) {
#else
static struct stream_to_id_map *splay(struct stream_to_id_map *root, CUstream key) {
#endif
    REGULAR_SPLAY_TREE(stream_to_id_map, root, key, stream, left, right);
    return root;
}

static void CloseAllStreams(struct stream_to_id_map *root) {

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

#ifdef CUDA_RT_API
static uint32_t SplayGetStreamId(struct stream_to_id_map *root, cudaStream_t key) {
#else                           // Driver API
static uint32_t SplayGetStreamId(struct stream_to_id_map *root, cudaStream_t key) {
#endif
    spinlock_lock(&g_stream_id_lock);
    REGULAR_SPLAY_TREE(stream_to_id_map, root, key, stream, left, right);
    stream_to_id_tree_root = root;
    uint32_t ret = stream_to_id_tree_root->id;
    spinlock_unlock(&g_stream_id_lock);
    return ret;

}

#ifdef CUDA_RT_API

static stream_to_id_map *splay_insert(cudaStream_t stream_ip)
#else                           // Driver API
static stream_to_id_map *splay_insert(CUstream stream_ip)
#endif
{

    spinlock_lock(&g_stream_id_lock);
    struct stream_to_id_map *node = &stream_to_id[stream_to_id_index++];
    node->stream = stream_ip;
    node->left = node->right = NULL;
    node->id = g_stream_id++;
    cudaStream_t stream = node->stream;

    if (stream_to_id_tree_root != NULL) {
        stream_to_id_tree_root = splay(stream_to_id_tree_root, stream);

        if (stream < stream_to_id_tree_root->stream) {
            node->left = stream_to_id_tree_root->left;
            node->right = stream_to_id_tree_root;
            stream_to_id_tree_root->left = NULL;
        } else if (stream > stream_to_id_tree_root->stream) {
            node->left = stream_to_id_tree_root;
            node->right = stream_to_id_tree_root->right;
            stream_to_id_tree_root->right = NULL;
        } else {
            TMSG(MEMLEAK, "stream_to_id_tree_root  splay tree: unable to insert %p (already present)", node->stream);
            assert(0);
        }
    }
    stream_to_id_tree_root = node;
    spinlock_unlock(&g_stream_id_lock);
    return stream_to_id_tree_root;
}

#ifdef CUDA_RT_API
static struct stream_to_id_map *splay_delete(cudaStream_t stream)
#else
static struct stream_to_id_map *splay_delete(CUstream stream)
#endif
{
    struct stream_to_id_map *result = NULL;

    spinlock_lock(&g_stream_id_lock);
    if (stream_to_id_tree_root == NULL) {
        spinlock_unlock(&g_stream_id_lock);
        TMSG(MEMLEAK, "stream_to_id_map splay tree empty: unable to delete %p", stream);
        return NULL;
    }

    stream_to_id_tree_root = splay(stream_to_id_tree_root, stream);

    if (stream != stream_to_id_tree_root->stream) {
        spinlock_unlock(&g_stream_id_lock);
        TMSG(MEMLEAK, "memleak splay tree: %p not in tree", stream);
        assert(0 && "deleting non existing node in splay tree");
        return NULL;
    }

    result = stream_to_id_tree_root;

    if (stream_to_id_tree_root->left == NULL) {
        stream_to_id_tree_root = stream_to_id_tree_root->right;
        spinlock_unlock(&g_stream_id_lock);
        return result;
    }

    stream_to_id_tree_root->left = splay(stream_to_id_tree_root->left, stream);
    stream_to_id_tree_root->left->right = stream_to_id_tree_root->right;
    stream_to_id_tree_root = stream_to_id_tree_root->left;
    spinlock_unlock(&g_stream_id_lock);
    return result;
}

static inline event_list_node *EnterCudaSync(uint64_t * syncStart) {
    /// caller does HPCRUN_ASYNC_BLOCK_SPIN_LOCK;

    // Cleanup events so that when I goto wait anybody in the queue will be the ones I have not seen and finished after my timer started.
    cleanup_finished_events();

    struct timeval tv;
    gettimeofday(&tv, NULL);
    *syncStart = ((uint64_t) tv.tv_usec + (((uint64_t) tv.tv_sec) * 1000000));

    //fprintf(stderr, "\n Start of world = %lu", g_start_of_world_time);
    //fprintf(stderr, "\n Sync start at  = %lu", *syncStart);
    event_list_node *recorded_node = g_finished_event_nodes_tail;
    if (g_finished_event_nodes_tail != &dummy_event_node)
        g_finished_event_nodes_tail->ref_count++;

    atomic_add_i64(&g_num_threads_at_sync, 1L);
    // caller does  HPCRUN_ASYNC_UNBLOCK_SPIN_UNLOCK;
    return recorded_node;
}

static uint64_t attribute_shared_blame_on_kernels(event_list_node * recorded_node, uint64_t recorded_time, const uint32_t stream_mask) {

    // if recorded_node is not dummy_event_node decrement its ref count
    if (recorded_node != &dummy_event_node)
        recorded_node->ref_count--;

    uint32_t num_active_kernels = 0;
    active_kernel_node *sorted_active_kernels_begin = NULL;

    // Traverse all nodes, inserting them in a sorted list if their end times were past the recorded time
    // If their start times were before the recorded, just record them as recorded_time

    event_list_node *cur = recorded_node->next, *prev = recorded_node;
    while (cur != &dummy_event_node) {
        // if the node's refcount is already zero, then free it and we dont care about it
        if (cur->ref_count == 0) {
            prev->next = cur->next;
            event_list_node *to_free = cur;
            cur = cur->next;
            ADD_TO_FREE_EVENTS_LIST(to_free);
            continue;
        }

        cur->ref_count--;

        if ((cur->event_end_time <= recorded_time) || (cur->stream_id != (cur->stream_id & stream_mask))) {
            if (cur->ref_count == 0) {
                prev->next = cur->next;
                event_list_node *to_free = cur;
                cur = cur->next;
                ADD_TO_FREE_EVENTS_LIST(to_free);
            } else {
                prev = cur;
                cur = cur->next;
            }
            continue;
        }
        // Add start and end times in a sorted list (insertion sort)

        active_kernel_node *start_active_kernel_node;
        active_kernel_node *end_active_kernel_node;
        GET_NEW_ACTIVE_KERNEL_NODE(start_active_kernel_node);
        GET_NEW_ACTIVE_KERNEL_NODE(end_active_kernel_node);

        if (cur->event_start_time < recorded_time) {
            start_active_kernel_node->event_time = recorded_time;
        } else {
            start_active_kernel_node->event_time = cur->event_start_time;
        }

        start_active_kernel_node->event_type = KERNEL_START;
        start_active_kernel_node->stream_id = cur->stream_id;
        start_active_kernel_node->launcher_cct = cur->launcher_cct;
        start_active_kernel_node->next_active_kernel = NULL;

        end_active_kernel_node->event_type = KERNEL_END;
        end_active_kernel_node->start_node = start_active_kernel_node;
        end_active_kernel_node->event_time = cur->event_end_time;

        // drop if times are same 
        if (start_active_kernel_node->event_time == end_active_kernel_node->event_time) {
            ADD_TO_FREE_ACTIVE_KERNEL_NODE_LIST(start_active_kernel_node);
            ADD_TO_FREE_ACTIVE_KERNEL_NODE_LIST(end_active_kernel_node);
            if (cur->ref_count == 0) {
                prev->next = cur->next;
                event_list_node *to_free = cur;
                cur = cur->next;
                ADD_TO_FREE_EVENTS_LIST(to_free);
            } else {
                prev = cur;
                cur = cur->next;
            }
            continue;
        }
        assert(start_active_kernel_node->event_time < end_active_kernel_node->event_time);

        if (sorted_active_kernels_begin == NULL) {
            // First entry
            start_active_kernel_node->next = end_active_kernel_node;
            start_active_kernel_node->prev = end_active_kernel_node;
            end_active_kernel_node->prev = start_active_kernel_node;
            end_active_kernel_node->next = start_active_kernel_node;
            sorted_active_kernels_begin = start_active_kernel_node;
        } else {
            // There are atlest 2 entries

            // current points to the last node interms of time  
            active_kernel_node *current = sorted_active_kernels_begin->prev;
            bool change_head = 1;
            do {
                if (end_active_kernel_node->event_time > current->event_time) {
                    change_head = 0;
                    break;
                }
                current = current->prev;
            } while (current != sorted_active_kernels_begin->prev);
            end_active_kernel_node->next = current->next;
            end_active_kernel_node->prev = current;
            current->next->prev = end_active_kernel_node;
            current->next = end_active_kernel_node;
            if (change_head) {
                sorted_active_kernels_begin = end_active_kernel_node;
            }

            current = end_active_kernel_node->prev;
            change_head = 1;
            do {
                if (start_active_kernel_node->event_time > current->event_time) {
                    change_head = 0;
                    break;
                }
                current = current->prev;
            } while (current != sorted_active_kernels_begin->prev);
            start_active_kernel_node->next = current->next;
            start_active_kernel_node->prev = current;
            current->next->prev = start_active_kernel_node;
            current->next = start_active_kernel_node;
            if (change_head) {
                sorted_active_kernels_begin = start_active_kernel_node;
            }
        }

        if (cur->ref_count == 0) {
            prev->next = cur->next;
            event_list_node *to_free = cur;
            cur = cur->next;
            ADD_TO_FREE_EVENTS_LIST(to_free);
        } else {
            prev = cur;
            cur = cur->next;
        }

    }
    g_finished_event_nodes_tail = prev;

    // now attribute blame on the sorted list
    uint64_t last_kernel_end_time = 0;
    if (sorted_active_kernels_begin) {

        // attach a dummy tail 
        active_kernel_node *dummy_kernel_node;
        GET_NEW_ACTIVE_KERNEL_NODE(dummy_kernel_node);
        sorted_active_kernels_begin->prev->next = dummy_kernel_node;
        dummy_kernel_node->prev = sorted_active_kernels_begin->prev;
        sorted_active_kernels_begin->prev = dummy_kernel_node;
        dummy_kernel_node->next = sorted_active_kernels_begin;

        active_kernel_node *current = sorted_active_kernels_begin;
        uint64_t last_time = recorded_time;
        do {
            uint64_t new_time = current->event_time;

            assert(new_time >= last_time);
            assert(current != dummy_kernel_node && "should never process dummy_kernel_node");

            if (num_active_kernels && (new_time > last_time)) {
                //blame all 
                active_kernel_node *blame_node = current->prev;
                do {
                    assert(blame_node->event_type == KERNEL_START);

                    cct_metric_data_increment(cpu_idle_cause_metric_id, blame_node->launcher_cct, (cct_metric_data_t) {
                                              .r = (new_time - last_time) * 1.0 / num_active_kernels}
                    );
                    blame_node = blame_node->prev;
                } while (blame_node != sorted_active_kernels_begin->prev);
            }

            last_time = new_time;

            if (current->event_type == KERNEL_START) {
                num_active_kernels++;
                current = current->next;
            } else {
                last_kernel_end_time = new_time;
                current->start_node->prev->next = current->start_node->next;
                current->start_node->next->prev = current->start_node->prev;
                if (current->start_node == sorted_active_kernels_begin)
                    sorted_active_kernels_begin = current->start_node->next;
                ADD_TO_FREE_ACTIVE_KERNEL_NODE_LIST((current->start_node));

#if 0                           // Not a plausible case
                // If I am the last one then Just free and break;
                if (current->next == current) {
                    ADD_TO_FREE_ACTIVE_KERNEL_NODE_LIST(current);
                    break;
                }
#endif
                current->prev->next = current->next;
                current->next->prev = current->prev;
                if (current == sorted_active_kernels_begin)
                    sorted_active_kernels_begin = current->next;
                num_active_kernels--;
                active_kernel_node *to_free = current;
                current = current->next;
                ADD_TO_FREE_ACTIVE_KERNEL_NODE_LIST(to_free);

            }

        } while (current != sorted_active_kernels_begin->prev);
        // Free up the dummy node
        ADD_TO_FREE_ACTIVE_KERNEL_NODE_LIST(dummy_kernel_node);

    }

    return last_kernel_end_time;

}

static inline uint64_t LeaveCudaSync(event_list_node * recorded_node, uint64_t syncStart, const uint32_t stream_mask) {
    //caller does       HPCRUN_ASYNC_BLOCK_SPIN_LOCK;

    // Cleanup events so that when I goto wait anybody in the queue will be the ones I have not seen and finished after my timer started.
    cleanup_finished_events();

    uint64_t last_kernel_end_time = attribute_shared_blame_on_kernels(recorded_node, syncStart, stream_mask);
    atomic_add_i64(&g_num_threads_at_sync, -1L);
    return last_kernel_end_time;
    //caller does       HPCRUN_ASYNC_UNBLOCK_SPIN_UNLOCK;
}

static uint32_t cleanup_finished_events() {
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
        assert(cur_stream->unfinished_event_node && " Can't point unfinished stream to null");
        next_stream = cur_stream->next_unfinished_stream;

        event_list_node *current_event = cur_stream->unfinished_event_node;
        while (current_event) {

            //cudaError_t err_cuda = cudaErrorNotReady;
            //fprintf(stderr, "\n cudaEventQuery on  %p", current_event->event_end);
            //fflush(stdout);

#ifdef CUDA_RT_API
            cudaError_t err_cuda = cudaEventQuery(current_event->event_end);
#else
            CUresult err_cuda = cuEventQuery(current_event->event_end);
#endif

            if (err_cuda == cudaSuccess) {
                //fprintf(stderr, "\n cudaEventQuery success %p", current_event->event_end);
                cct_node_t *launcher_cct = current_event->launcher_cct;
                hpcrun_cct_persistent_id_trace_mutate(launcher_cct);
                // record start time
                float elapsedTime;      // in millisec with 0.5 microsec resolution as per CUDA

                //FIX ME: deleting Elapsed time to handle context destruction.... 
                //static uint64_t deleteMeTime = 0;
#ifdef CUDA_RT_API
                CUDA_SAFE_CALL(cudaRuntimeFunctionPointer[CUDA_EVENT_ELAPSED_TIME].cudaEventElapsedTimeReal(&elapsedTime, g_start_of_world_event, current_event->event_start));
#else
                CU_SAFE_CALL(cuDriverFunctionPointer[CU_EVENT_ELAPSED_TIME].cuEventElapsedTimeReal(&elapsedTime, g_start_of_world_event, current_event->event_start));

#endif
                assert(elapsedTime > 0);

                uint64_t micro_time_start = (uint64_t) (((double) elapsedTime) * 1000) + g_start_of_world_time;
#if 0
                fprintf(stderr, "\n started at  :  %lu", micro_time_start);
#endif

                gpu_trace_append_with_time(cur_stream->st, DEVICE_ID, GET_STREAM_ID(cur_stream), cur_stream->st->idle_node_id, micro_time_start - 1);

                cct_node_t *stream_cct = current_event->stream_launcher_cct;
                hpcrun_cct_persistent_id_trace_mutate(stream_cct);

                //printf("\n adding stream_cct:%x with id:%d",stream_cct, hpcrun_cct_persistent_id(stream_cct)); 
                gpu_trace_append_with_time(cur_stream->st, DEVICE_ID, GET_STREAM_ID(cur_stream), hpcrun_cct_persistent_id(stream_cct), micro_time_start);

                // record end time
                // TODO : WE JUST NEED TO PUT IDLE MARKER

#ifdef CUDA_RT_API
                CUDA_SAFE_CALL(cudaRuntimeFunctionPointer[CUDA_EVENT_ELAPSED_TIME].cudaEventElapsedTimeReal(&elapsedTime, g_start_of_world_event, current_event->event_end));
#else
                CU_SAFE_CALL(cuDriverFunctionPointer[CU_EVENT_ELAPSED_TIME].cuEventElapsedTimeReal(&elapsedTime, g_start_of_world_event, current_event->event_end));
#endif
                assert(elapsedTime > 0);
                uint64_t micro_time_end = (uint64_t) (((double) elapsedTime) * 1000) + g_start_of_world_time;
#if 0
                fprintf(stderr, "\n Ended  at  :  %lu", micro_time_end);
#endif

                //printf("\n adding stream_cct:%x with id:%d",stream_cct, hpcrun_cct_persistent_id(stream_cct)); 
                gpu_trace_append_with_time(cur_stream->st, DEVICE_ID, GET_STREAM_ID(cur_stream), hpcrun_cct_persistent_id(stream_cct), micro_time_end);

                // TODO : WE JUST NEED TO PUT IDLE MARKER
                gpu_trace_append_with_time(cur_stream->st, DEVICE_ID, GET_STREAM_ID(cur_stream), cur_stream->st->idle_node_id, micro_time_end + 1);
#if 0

                err_cuda = cudaEventElapsedTime(&elapsedTime, current_event->event_start, current_event->event_end);
                //fprintf(stderr, "\n Length of Event :  %f", elapsedTime);

#endif

                assert(micro_time_start <= micro_time_end);

                // Add the kernel execution time to the gpu_activity_time_metric_id
                cct_metric_data_increment(gpu_activity_time_metric_id, current_event->launcher_cct, (cct_metric_data_t) {
                                          .i = (micro_time_end - micro_time_start)});

                event_list_node *deferred_node = current_event;
                current_event = current_event->next;
                // Add to_free to fre list 

                if (g_num_threads_at_sync) {
                    // some threads are waiting, hence add this kernel for deferred blaming
                    deferred_node->ref_count = g_num_threads_at_sync;
                    deferred_node->event_start_time = micro_time_start;
                    deferred_node->event_end_time = micro_time_end;
                    deferred_node->next = g_finished_event_nodes_tail->next;
                    g_finished_event_nodes_tail->next = deferred_node;
                    g_finished_event_nodes_tail = deferred_node;

                } else {
                    // TODO : destroy events            
                    ADD_TO_FREE_EVENTS_LIST(deferred_node);
                }

            } else {
                //fprintf(stderr, "\n cudaEventQuery failed %p", current_event->event_end);
                break;
            }
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

            num_unfinished_streams++;
            prev_stream = cur_stream;
        }
        cur_stream = next_stream;
    }
    return num_unfinished_streams;
}

static event_list_node *create_and_insert_event(int stream_id, cct_node_t * launcher_cct, cct_node_t * stream_launcher_cct) {

    event_list_node *event_node;
    if (g_free_event_nodes_head) {
        // get from free list
        event_node = g_free_event_nodes_head;
        // Destroy those old used ones 
        assert(event_node->event_start);

#ifdef CUDA_RT_API
        CUDA_SAFE_CALL(cudaEventDestroy(event_node->event_start));
#else                           // Driver API
        ////////TODO: FIX ME CANT DELETE IF STREAM IS DELETED       err =  cuEventDestroy(event_node->event_start);
        ///CHECK_CU_ERROR(err, "cudaEventDestroy");
#endif

        assert(event_node->event_end);

#ifdef CUDA_RT_API
        CUDA_SAFE_CALL(cudaEventDestroy(event_node->event_end));
#else                           // Driver API
        ///// TODO: FIX ME: CANT DELETE AFTER STREAM IS DELETED        err = cuEventDestroy(event_node->event_end);
        ///     CHECK_CU_ERROR(err, "cudaEventDestroy");
#endif
        g_free_event_nodes_head = g_free_event_nodes_head->next_free_node;
    } else {
        // allocate new node
        event_node = (event_list_node *) hpcrun_malloc(sizeof(event_list_node));
    }
    //cudaError_t err =  cudaEventCreateWithFlags(&(event_node->event_end),cudaEventDisableTiming);

#ifdef CUDA_RT_API
    CUDA_SAFE_CALL(cudaRuntimeFunctionPointer[CUDA_EVENT_CREATE].cudaEventCreateReal(&(event_node->event_start)));
#else
    CU_SAFE_CALL(cuDriverFunctionPointer[CU_EVENT_CREATE].cuEventCreateReal(&(event_node->event_start), CU_EVENT_DEFAULT));
#endif

#ifdef CUDA_RT_API
    CUDA_SAFE_CALL(cudaRuntimeFunctionPointer[CUDA_EVENT_CREATE].cudaEventCreateReal(&(event_node->event_end)));
#else
    CU_SAFE_CALL(cuDriverFunctionPointer[CU_EVENT_CREATE].cuEventCreateReal(&(event_node->event_end), CU_EVENT_DEFAULT));
#endif

    event_node->stream_launcher_cct = stream_launcher_cct;
    event_node->launcher_cct = launcher_cct;
    event_node->next = NULL;
    event_node->stream_id = stream_id;
    if (g_stream_array[stream_id].latest_event_node == NULL) {
        g_stream_array[stream_id].latest_event_node = event_node;
        g_stream_array[stream_id].unfinished_event_node = event_node;
        g_stream_array[stream_id].next_unfinished_stream = g_unfinished_stream_list_head;
        g_unfinished_stream_list_head = &(g_stream_array[stream_id]);
    } else {
        g_stream_array[stream_id].latest_event_node->next = event_node;
        g_stream_array[stream_id].latest_event_node = event_node;
    }

    return event_node;
}

#ifdef CUDA_RT_API
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
#else
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
#endif

static void PopulateEntryPointesToWrappedCalls() {
#ifdef CUDA_RT_API
    PopulateEntryPointesToWrappedCudaRuntimeCalls();
#else
    PopulateEntryPointesToWrappedCuDriverCalls();
#endif
}

void gpu_blame_shift_init() {
    g_unfinished_stream_list_head = NULL;
    g_finished_event_nodes_tail = &dummy_event_node;
    dummy_event_node.next = g_finished_event_nodes_tail;
    PopulateEntryPointesToWrappedCalls();
}

void gpu_blame_shift_thread_init() {
    atomic_add_i64(&g_active_threads, 1L);
}

void gpu_blame_shift_thread_fini_action() {

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

void gpu_blame_shift_shutdown() {

#if 0
    if (!g_stream0_not_initialized) {
        fprintf(stderr, "\n MAY BE BROKEN FOR MULTITHREADED");
        uint32_t streamId;
        streamId = SplayGetStreamId(stream_to_id_tree_root, 0);
        SYNCHRONOUS_CLEANUP;
        gpu_trace_close(DEVICE_ID, streamId);
    }
#endif
}

void gpu_blame_shift_process_event_list(int metric_id) {
    wall_clock_metric_id = metric_id;
    /*Creating a dummy metric for stream ccts */
    stream_special_metric_id = hpcrun_new_metric();
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

    /*creating a dummy metric for stream ccts */
    hpcrun_set_metric_info_and_period(stream_special_metric_id, "STREAM SPECIAL (us)", MetricFlags_ValFmt_Int, 1);
    hpcrun_set_metric_info_and_period(cpu_idle_metric_id, "CPU_IDLE", MetricFlags_ValFmt_Int, 1);
    hpcrun_set_metric_info_and_period(gpu_idle_metric_id, "GPU_IDLE_CAUSE", MetricFlags_ValFmt_Int, 1);
    hpcrun_set_metric_info_and_period(cpu_idle_cause_metric_id, "CPU_IDLE_CAUSE", MetricFlags_ValFmt_Real, 1);
    hpcrun_set_metric_info_and_period(cpu_overlap_metric_id, "OVERLAPPED_CPU", MetricFlags_ValFmt_Real, 1);
    hpcrun_set_metric_info_and_period(gpu_overlap_metric_id, "OVERLAPPED_GPU", MetricFlags_ValFmt_Real, 1);
    hpcrun_set_metric_info_and_period(gpu_activity_time_metric_id, "GPU_ACTIVITY_TIME", MetricFlags_ValFmt_Int, 1);
}

#ifdef CUDA_RT_API
void CreateStream0IfNot(cudaStream_t stream) {
#else
void CreateStream0IfNot(CUstream stream) {
#endif                          // if stream is 0 and if it is not created, then create one:
    HPCRUN_ASYNC_BLOCK_SPIN_LOCK;
    if ((uint64_t) stream == 0 && g_stream0_not_initialized) {
        uint32_t new_streamId;
        new_streamId = splay_insert(0)->id;
        fprintf(stderr, "\n Stream id = %d", new_streamId);
        if (g_start_of_world_time == 0) {
            // And disable tracking new threads from CUDA
            monitor_disable_new_threads();

            // Initialize and Record an event to indicate the start of this stream.
            // No need to wait for it since we query only the events posted after this and this will be complete when the latter posted ones are complete.

#ifdef CUDA_RT_API
            CUDA_SAFE_CALL(cudaRuntimeFunctionPointer[CUDA_EVENT_CREATE].cudaEventCreateReal(&g_start_of_world_event));
#else
            CU_SAFE_CALL(cuDriverFunctionPointer[CU_EVENT_CREATE].cuEventCreateReal(&g_start_of_world_event, CU_EVENT_DEFAULT));
#endif

            // record time

            struct timeval tv;
            gettimeofday(&tv, NULL);
            g_start_of_world_time = ((uint64_t) tv.tv_usec + (((uint64_t) tv.tv_sec) * 1000000));

            // record in stream 0       
#ifdef CUDA_RT_API
            CUDA_SAFE_CALL(cudaRuntimeFunctionPointer[CUDA_EVENT_RECORD].cudaEventRecordReal(g_start_of_world_event, 0));
#else
            CU_SAFE_CALL(cuDriverFunctionPointer[CU_EVENT_RECORD].cuEventRecordReal(g_start_of_world_event, 0));
#endif

            // enable monitoring new threads
            monitor_enable_new_threads();
        }

        struct timeval tv;
        gettimeofday(&tv, NULL);
        g_stream_array[new_streamId].st = hpcrun_stream_data_alloc_init(DEVICE_ID, new_streamId);
        gpu_trace_open(g_stream_array[new_streamId].st, DEVICE_ID, new_streamId);

        /*FIXME: convert below 4 lines to a macro */
        cct_bundle_t *bundle = &(g_stream_array[new_streamId].st->epoch->csdata);
        cct_node_t *idl = hpcrun_cct_bundle_get_idle_node(bundle);
        hpcrun_cct_persistent_id_trace_mutate(idl);
        // store the persistent id one time
        g_stream_array[new_streamId].st->idle_node_id = hpcrun_cct_persistent_id(idl);

        gpu_trace_append(g_stream_array[new_streamId].st, DEVICE_ID, new_streamId, g_stream_array[new_streamId].st->idle_node_id);

        g_stream0_not_initialized = false;
    }
    HPCRUN_ASYNC_UNBLOCK_SPIN_UNLOCK;

}

#ifdef CUDA_RT_API
cudaError_t cudaThreadSynchronize(void) {

    SYNC_PROLOGUE(context, launcher_cct, syncStart, recorded_node);

    cudaError_t ret = cudaRuntimeFunctionPointer[CUDA_THREAD_SYNCHRONIZE].cudaThreadSynchronizeReal();

    SYNC_EPILOGUE(context, launcher_cct, syncStart, recorded_node, ALL_STREAMS_MASK, syncEnd);
    return ret;
}

cudaError_t cudaStreamSynchronize(cudaStream_t stream) {
    SYNC_PROLOGUE(context, launcher_cct, syncStart, recorded_node);

    cudaError_t ret = cudaRuntimeFunctionPointer[CUDA_STREAM_SYNCHRONIZE].cudaStreamSynchronizeReal(stream);

    hpcrun_async_block();
    uint32_t streamId;
    streamId = SplayGetStreamId(stream_to_id_tree_root, stream);
    hpcrun_async_unblock();

    SYNC_EPILOGUE(context, launcher_cct, syncStart, recorded_node, streamId, syncEnd);
    return ret;
}

cudaError_t cudaEventSynchronize(cudaEvent_t event) {
    SYNC_PROLOGUE(context, launcher_cct, syncStart, recorded_node);

    cudaError_t ret = cudaRuntimeFunctionPointer[CUDA_EVENT_SYNCHRONIZE].cudaEventSynchronizeReal(event);

    SYNC_EPILOGUE(context, launcher_cct, syncStart, recorded_node, ALL_STREAMS_MASK, syncEnd);

    return ret;
}

cudaError_t cudaStreamWaitEvent(cudaStream_t stream, cudaEvent_t event, unsigned int flags) {
    SYNC_PROLOGUE(context, launcher_cct, syncStart, recorded_node);

    cudaError_t ret = cudaRuntimeFunctionPointer[CUDA_STREAM_WAIT_EVENT].cudaStreamWaitEventReal(stream, event, flags);

    hpcrun_async_block();
    uint32_t streamId;
    streamId = SplayGetStreamId(stream_to_id_tree_root, stream);
    hpcrun_async_unblock();

    SYNC_EPILOGUE(context, launcher_cct, syncStart, recorded_node, streamId, syncEnd);

    return ret;
}

cudaError_t cudaDeviceSynchronize(void) {
    SYNC_PROLOGUE(context, launcher_cct, syncStart, recorded_node);

    cudaError_t ret = cudaRuntimeFunctionPointer[CUDA_DEVICE_SYNCHRONIZE].cudaDeviceSynchronizeReal();

    SYNC_EPILOGUE(context, launcher_cct, syncStart, recorded_node, ALL_STREAMS_MASK, syncEnd);

    return ret;
}

cudaError_t cudaConfigureCall(dim3 grid, dim3 block, size_t mem, cudaStream_t stream) {
    TD_GET(is_thread_at_cuda_sync) = true;

    cudaError_t ret = cudaRuntimeFunctionPointer[CUDA_CONFIGURE_CALL].cudaConfigureCallReal(grid, block, mem, stream);

    TD_GET(is_thread_at_cuda_sync) = false;

    TD_GET(active_stream) = (uint64_t) stream;

    CreateStream0IfNot(stream);

    return ret;
}

cudaError_t cudaLaunch(const char *entry) {
    uint32_t streamId = 0;
    event_list_node *event_node;

    streamId = SplayGetStreamId(stream_to_id_tree_root, (cudaStream_t) (TD_GET(active_stream)));
    // We cannot allow this thread to take samples since we will be holding a lock which is also needed in the async signal handler
    // let no other GPU work get ahead of me
    HPCRUN_ASYNC_BLOCK_SPIN_LOCK;
    // And disable tracking new threads from CUDA
    monitor_disable_new_threads();
    // Get CCT node (i.e., kernel launcher)
    ucontext_t context;
    getcontext(&context);
    // skipping 2 inner for LAMMPS
    cct_node_t *node = hpcrun_sample_callpath(&context, cpu_idle_metric_id, 0, 2 /*skipInner */ , 1 /*isSync */ );
    // Launcher CCT node will be 3 levels above in the loaded module ( Handler -> CUpti -> Cuda -> Launcher )
    // TODO: Get correct level .. 3 worked but not on S3D
    ////node = hpcrun_get_cct_node_n_levels_up_in_load_module(node, 0);

    // correct node is not simple to find ....
    //node =  hpcrun_cct_parent(hpcrun_cct_parent(hpcrun_cct_parent(node)));
    //node =  hpcrun_cct_parent(hpcrun_cct_parent(node));

    /* FIXME: we are about to insert the cct into the stream 
     * we use the context above, then get teh backtrace, insert the backtrace
     * to the stream->epoch->csdata
     */
    cct_node_t *stream_cct = stream_backtrace2cct(g_stream_array[streamId].st, &context);

    // Create a new Cuda Event
    //cuptiGetStreamId(ctx, (CUstream) TD_GET(active_stream), &streamId);
    event_node = create_and_insert_event(streamId, node, stream_cct);
    // Insert the event in the stream
    //      assert(TD_GET(active_stream));
    fprintf(stderr, "\n start on stream = %p ", TD_GET(active_stream));
    CUDA_SAFE_CALL(cudaRuntimeFunctionPointer[CUDA_EVENT_RECORD].cudaEventRecordReal(event_node->event_start, (cudaStream_t) TD_GET(active_stream)));

    cudaError_t ret = cudaRuntimeFunctionPointer[CUDA_LAUNCH].cudaLaunchReal(entry);

    fprintf(stderr, "\n end  on stream = %p ", TD_GET(active_stream));
    CUDA_SAFE_CALL(cudaRuntimeFunctionPointer[CUDA_EVENT_RECORD].cudaEventRecordReal(event_node->event_end, (cudaStream_t) TD_GET(active_stream)));
    // enable monitoring new threads
    monitor_enable_new_threads();
    // let other things be queued into GPU
    // safe to take async samples now
    HPCRUN_ASYNC_UNBLOCK_SPIN_UNLOCK;

    return ret;
}

cudaError_t cudaStreamDestroy(cudaStream_t stream) {

    SYNCHRONOUS_CLEANUP;

    hpcrun_async_block();

    uint32_t streamId;

    streamId = SplayGetStreamId(stream_to_id_tree_root, stream);

    struct timeval tv;
    gettimeofday(&tv, NULL);
    gpu_trace_close(g_stream_array[streamId].st, DEVICE_ID, streamId);
    hpcrun_stream_finalize(g_stream_array[streamId].st);
    g_stream_array[streamId].st = NULL;

    cudaError_t ret = cudaRuntimeFunctionPointer[CUDA_STREAM_DESTROY].cudaStreamDestroyReal(stream);

    // Delete splay tree entry
    splay_delete(stream);
    hpcrun_async_unblock();
    return ret;

}

cudaError_t cudaStreamCreate(cudaStream_t * stream) {

    TD_GET(is_thread_at_cuda_sync) = true;

    cudaError_t ret = cudaRuntimeFunctionPointer[CUDA_STREAM_CREATE].cudaStreamCreateReal(stream);

    TD_GET(is_thread_at_cuda_sync) = false;

    uint32_t new_streamId;

    new_streamId = splay_insert(*stream)->id;
    fprintf(stderr, "\n Stream id = %d", new_streamId);

    HPCRUN_ASYNC_BLOCK_SPIN_LOCK;
    if (g_start_of_world_time == 0) {
        // And disable tracking new threads from CUDA
        monitor_disable_new_threads();

        // Initialize and Record an event to indicate the start of this stream.
        // No need to wait for it since we query only the events posted after this and this will be complete when the latter posted ones are complete.
        CUDA_SAFE_CALL(cudaRuntimeFunctionPointer[CUDA_EVENT_CREATE].cudaEventCreateReal(&g_start_of_world_event));

        // record time

        struct timeval tv;
        gettimeofday(&tv, NULL);
        g_start_of_world_time = ((uint64_t) tv.tv_usec + (((uint64_t) tv.tv_sec) * 1000000));

        // record in stream 0       
        CUDA_SAFE_CALL(cudaRuntimeFunctionPointer[CUDA_EVENT_RECORD].cudaEventRecordReal(g_start_of_world_event, 0));

        // enable monitoring new threads
        monitor_enable_new_threads();
    }
    HPCRUN_ASYNC_UNBLOCK_SPIN_UNLOCK;

    g_stream_array[new_streamId].st = hpcrun_stream_data_alloc_init(DEVICE_ID, new_streamId);
    gpu_trace_open(g_stream_array[new_streamId].st, DEVICE_ID, new_streamId);

    /*FIXME: convert below 4 lines to a macro */
    cct_bundle_t *bundle = &(g_stream_array[new_streamId].st->epoch->csdata);
    cct_node_t *idl = hpcrun_cct_bundle_get_idle_node(bundle);
    hpcrun_cct_persistent_id_trace_mutate(idl);
    // store the persistent id one time.
    g_stream_array[new_streamId].st->idle_node_id = hpcrun_cct_persistent_id(idl);
    gpu_trace_append(g_stream_array[new_streamId].st, DEVICE_ID, new_streamId, g_stream_array[new_streamId].st->idle_node_id);

    return ret;
}

cudaError_t cudaMalloc(void **devPtr, size_t size) {

    SYNC_PROLOGUE(context, launcher_cct, syncStart, recorded_node);

    cudaError_t ret = cudaRuntimeFunctionPointer[CUDA_MALLOC].cudaMallocReal(devPtr, size);

    SYNC_EPILOGUE(context, launcher_cct, syncStart, recorded_node, ALL_STREAMS_MASK, syncEnd);

    return ret;

}

cudaError_t cudaFree(void *devPtr) {

    SYNC_PROLOGUE(context, launcher_cct, syncStart, recorded_node);

    cudaError_t ret = cudaRuntimeFunctionPointer[CUDA_FREE].cudaFreeReal(devPtr);

    SYNC_EPILOGUE(context, launcher_cct, syncStart, recorded_node, ALL_STREAMS_MASK, syncEnd);
    return ret;

}

cudaError_t cudaMemcpyAsync(void *dst, const void *src, size_t count, enum cudaMemcpyKind kind, cudaStream_t stream) {

    CreateStream0IfNot(stream);

    uint32_t streamId = 0;
    event_list_node *event_node;

    streamId = SplayGetStreamId(stream_to_id_tree_root, stream);
    // We cannot allow this thread to take samples since we will be holding a lock which is also needed in the async signal handler
    // let no other GPU work get ahead of me
    HPCRUN_ASYNC_BLOCK_SPIN_LOCK;
    // And disable tracking new threads from CUDA
    monitor_disable_new_threads();
    // Get CCT node (i.e., kernel launcher)
    ucontext_t context;
    getcontext(&context);
    cct_node_t *node = hpcrun_sample_callpath(&context, cpu_idle_metric_id, 0, 0 /*skipInner */ , 1 /*isSync */ );
    // Launcher CCT node will be 3 levels above in the loaded module ( Handler -> CUpti -> Cuda -> Launcher )
    // TODO: Get correct level .. 3 worked but not on S3D
    node = hpcrun_get_cct_node_n_levels_up_in_load_module(node, 0);

    /* FIXME: we are about to insert the cct into the stream 
     * we use the context above, then get teh backtrace, insert the backtrace
     * to the stream->epoch->csdata
     */
    cct_node_t *stream_cct = stream_backtrace2cct(g_stream_array[streamId].st, &context);

    // Create a new Cuda Event
    //cuptiGetStreamId(ctx, (CUstream) TD_GET(active_stream), &streamId);
    event_node = create_and_insert_event(streamId, node, stream_cct);
    // Insert the event in the stream
    //      assert(TD_GET(active_stream));
    fprintf(stderr, "\n start on stream = %p ", stream);
    CUDA_SAFE_CALL(cudaRuntimeFunctionPointer[CUDA_EVENT_RECORD].cudaEventRecordReal(event_node->event_start, stream));
    cudaError_t ret = cudaRuntimeFunctionPointer[CUDA_MEMCPY_ASYNC].cudaMemcpyAsyncReal(dst, src, count, kind, stream);

    fprintf(stderr, "\n end  on stream = %p ", stream);
    CUDA_SAFE_CALL(cudaRuntimeFunctionPointer[CUDA_EVENT_RECORD].cudaEventRecordReal(event_node->event_end, stream));
    // enable monitoring new threads
    monitor_enable_new_threads();
    // let other things be queued into GPU
    // safe to take async samples now
    HPCRUN_ASYNC_UNBLOCK_SPIN_UNLOCK;

    return ret;
}

cudaError_t cudaMemcpy2D(void *dst, size_t dpitch, const void *src, size_t spitch, size_t width, size_t height, enum cudaMemcpyKind kind) {

    SYNC_PROLOGUE(context, launcher_cct, syncStart, recorded_node);

    cudaError_t ret = cudaRuntimeFunctionPointer[CUDA_MEMCPY_2D].cudaMemcpy2DReal(dst, dpitch, src, spitch, width, height, kind);

    hpcrun_async_block();
    spinlock_lock(&g_gpu_lock);
    LeaveCudaSync(recorded_node, syncStart, ALL_STREAMS_MASK);
    spinlock_unlock(&g_gpu_lock);

    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t syncEnd = ((uint64_t) tv.tv_usec + (((uint64_t) tv.tv_sec) * 1000000));

    // this is both CPU and GPU idleness since one could have used cudaMemcpyAsync
    cct_metric_data_increment(cpu_idle_metric_id, launcher_cct, (cct_metric_data_t) {
                              .i = (syncEnd - syncStart)});
    cct_metric_data_increment(gpu_idle_metric_id, launcher_cct, (cct_metric_data_t) {
                              .i = (syncEnd - syncStart)});

    //idt += syncEnd - syncStart;
    //printf("\n gpu_idle_time = %lu .. %lu",syncEnd - syncStart,idt);

    hpcrun_async_unblock();

    TD_GET(is_thread_at_cuda_sync) = false;
    return ret;

}

cudaError_t cudaMemcpy(void *dst, const void *src, size_t count, enum cudaMemcpyKind kind) {

    SYNC_PROLOGUE(context, launcher_cct, syncStart, recorded_node);

    cudaError_t ret = cudaRuntimeFunctionPointer[CUDA_MEMCPY].cudaMemcpyReal(dst, src, count, kind);

    hpcrun_async_block();
    spinlock_lock(&g_gpu_lock);
    LeaveCudaSync(recorded_node, syncStart, ALL_STREAMS_MASK);
    spinlock_unlock(&g_gpu_lock);

    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t syncEnd = ((uint64_t) tv.tv_usec + (((uint64_t) tv.tv_sec) * 1000000));

    // this is both CPU and GPU idleness since one could have used cudaMemcpyAsync
    cct_metric_data_increment(cpu_idle_metric_id, launcher_cct, (cct_metric_data_t) {
                              .i = (syncEnd - syncStart)});
    cct_metric_data_increment(gpu_idle_metric_id, launcher_cct, (cct_metric_data_t) {
                              .i = (syncEnd - syncStart)});
    //idt += syncEnd - syncStart;
    //printf("\n gpu_idle_time = %lu .. %lu",syncEnd - syncStart,idt);

    hpcrun_async_unblock();

    TD_GET(is_thread_at_cuda_sync) = false;
    return ret;

}

cudaError_t cudaEventElapsedTime(float *ms, cudaEvent_t start, cudaEvent_t end) {

    TD_GET(is_thread_at_cuda_sync) = true;
    cudaError_t ret = cudaRuntimeFunctionPointer[CUDA_EVENT_ELAPSED_TIME].cudaEventElapsedTimeReal(ms, start, end);
    TD_GET(is_thread_at_cuda_sync) = false;
    return ret;
}

cudaError_t cudaEventCreate(cudaEvent_t * event) {
    TD_GET(is_thread_at_cuda_sync) = true;
    cudaError_t ret = cudaRuntimeFunctionPointer[CUDA_EVENT_CREATE].cudaEventCreateReal(event);
    TD_GET(is_thread_at_cuda_sync) = false;
    return ret;
}

cudaError_t cudaEventRecord(cudaEvent_t event, cudaStream_t stream) {
    TD_GET(is_thread_at_cuda_sync) = true;
    cudaError_t ret = cudaRuntimeFunctionPointer[CUDA_EVENT_RECORD].cudaEventRecordReal(event, stream);
    TD_GET(is_thread_at_cuda_sync) = false;
    return ret;
}

#else                           // Driver APIs

CUresult cuStreamSynchronize(CUstream stream) {
    SYNC_PROLOGUE(context, launcher_cct, syncStart, recorded_node);

    CUresult ret = cuDriverFunctionPointer[CU_STREAM_SYNCHRONIZE].cuStreamSynchronizeReal(stream);

    hpcrun_async_block();
    uint32_t streamId;
    streamId = SplayGetStreamId(stream_to_id_tree_root, stream);
    hpcrun_async_unblock();

    SYNC_EPILOGUE(context, launcher_cct, syncStart, recorded_node, streamId, syncEnd);

    return ret;
}

CUresult cuEventSynchronize(CUevent event) {
    SYNC_PROLOGUE(context, launcher_cct, syncStart, recorded_node);

    CUresult ret = cuDriverFunctionPointer[CU_EVENT_SYNCHRONIZE].cuEventSynchronizeReal(event);

    SYNC_EPILOGUE(context, launcher_cct, syncStart, recorded_node, ALL_STREAMS_MASK, syncEnd);

    return ret;
}

CUresult cuLaunchGridAsync(CUfunction f, int grid_width, int grid_height, CUstream hStream) {
    uint32_t streamId = 0;
    event_list_node *event_node;

    streamId = SplayGetStreamId(stream_to_id_tree_root, hStream);
    // We cannot allow this thread to take samples since we will be holding a lock which is also needed in the async signal handler
    // let no other GPU work get ahead of me
    HPCRUN_ASYNC_BLOCK_SPIN_LOCK;
    // And disable tracking new threads from CUDA
    monitor_disable_new_threads();
    // Get CCT node (i.e., kernel launcher)
    ucontext_t context;
    getcontext(&context);
    // skipping 2 inner for LAMMPS
    cct_node_t *node = hpcrun_sample_callpath(&context, cpu_idle_metric_id, 0, 2 /*skipInner */ , 1 /*isSync */ );
    // Launcher CCT node will be 3 levels above in the loaded module ( Handler -> CUpti -> Cuda -> Launcher )
    // TODO: Get correct level .. 3 worked but not on S3D
    ////node = hpcrun_get_cct_node_n_levels_up_in_load_module(node, 0);

    // correct node is not simple to find ....
    //node =  hpcrun_cct_parent(hpcrun_cct_parent(hpcrun_cct_parent(node)));
    //node =  hpcrun_cct_parent(hpcrun_cct_parent(node));

    /* FIXME: we are about to insert the cct into the stream 
     * we use the context above, then get teh backtrace, insert the backtrace
     * to the stream->epoch->csdata
     */
    cct_node_t *stream_cct = stream_backtrace2cct(g_stream_array[streamId].st, &context);

    // Create a new Cuda Event
    //cuptiGetStreamId(ctx, (CUstream) TD_GET(active_stream), &streamId);
    event_node = create_and_insert_event(streamId, node, stream_cct);
    // Insert the event in the stream
    //      assert(TD_GET(active_stream));

    CU_SAFE_CALL(cuDriverFunctionPointer[CU_EVENT_RECORD].cuEventRecordReal(event_node->event_start, hStream));

    CUresult ret = cuDriverFunctionPointer[CU_LAUNCH_GRID_ASYNC].cuLaunchGridAsyncReal(f, grid_width, grid_height, hStream);

    CU_SAFE_CALL(cuDriverFunctionPointer[CU_EVENT_RECORD].cuEventRecordReal(event_node->event_end, hStream));

    // enable monitoring new threads
    monitor_enable_new_threads();
    // let other things be queued into GPU
    // safe to take async samples now
    HPCRUN_ASYNC_UNBLOCK_SPIN_UNLOCK;
    return ret;
}

CUresult cuStreamDestroy(CUstream stream) {

    SYNCHRONOUS_CLEANUP;
    hpcrun_async_block();

    uint32_t streamId;
    streamId = SplayGetStreamId(stream_to_id_tree_root, stream);

    struct timeval tv;
    gettimeofday(&tv, NULL);
    gpu_trace_close(g_stream_array[streamId].st, DEVICE_ID, streamId);
    hpcrun_stream_finalize(g_stream_array[streamId].st);
    g_stream_array[streamId].st = NULL;

    cudaError_t ret = cuDriverFunctionPointer[CU_STREAM_DESTROY].cuStreamDestroyReal(stream);

    // Delete splay tree entry
    splay_delete(stream);
    hpcrun_async_unblock();
    return ret;

}

CUresult cuStreamCreate(CUstream * phStream, unsigned int Flags) {

    TD_GET(is_thread_at_cuda_sync) = true;
    CUresult ret = cuDriverFunctionPointer[CU_STREAM_CREATE].cuStreamCreateReal(phStream, Flags);
    TD_GET(is_thread_at_cuda_sync) = false;

    uint32_t new_streamId;
    new_streamId = splay_insert((cudaStream_t) * phStream)->id;
    fprintf(stderr, "\n Stream id = %d", new_streamId);

    HPCRUN_ASYNC_BLOCK_SPIN_LOCK;
    if (g_start_of_world_time == 0) {
        // And disable tracking new threads from CUDA
        monitor_disable_new_threads();

        // Initialize and Record an event to indicate the start of this stream.
        // No need to wait for it since we query only the events posted after this and this will be complete when the latter posted ones are complete.

        CU_SAFE_CALL(cuDriverFunctionPointer[CU_EVENT_CREATE].cuEventCreateReal(&g_start_of_world_event, CU_EVENT_DEFAULT));

        // record time

        struct timeval tv;
        gettimeofday(&tv, NULL);
        g_start_of_world_time = ((uint64_t) tv.tv_usec + (((uint64_t) tv.tv_sec) * 1000000));

        // record in stream 0    

        CU_SAFE_CALL(cuDriverFunctionPointer[CU_EVENT_RECORD].cuEventRecordReal(g_start_of_world_event, 0));

        // enable monitoring new threads
        monitor_enable_new_threads();
    }
    HPCRUN_ASYNC_UNBLOCK_SPIN_UNLOCK;

    g_stream_array[new_streamId].st = hpcrun_stream_data_alloc_init(DEVICE_ID, new_streamId);
    gpu_trace_open(g_stream_array[new_streamId].st, DEVICE_ID, new_streamId);

    /*FIXME: convert below 4 lines to a macro */
    cct_bundle_t *bundle = &(g_stream_array[new_streamId].st->epoch->csdata);
    cct_node_t *idl = hpcrun_cct_bundle_get_idle_node(bundle);
    hpcrun_cct_persistent_id_trace_mutate(idl);
    // Store the persistent id for the idle node one time.
    g_stream_array[new_streamId].st->idle_node_id = hpcrun_cct_persistent_id(idl);

    gpu_trace_append(g_stream_array[new_streamId].st, DEVICE_ID, new_streamId, g_stream_array[new_streamId].st->idle_node_id);

    //cuptiGetStreamId(cbRDInfo->context, cbRDInfo->resourceHandle.stream, &new_streamId);
    //SYNCHRONOUS_CLEANUP;
    //gpu_trace_close(0, new_streamId);

    return ret;

}

CUresult cuCtxDestroy(CUcontext ctx) {

    SYNCHRONOUS_CLEANUP;

    HPCRUN_ASYNC_BLOCK_SPIN_LOCK;
    if (g_start_of_world_time != 0) {
        // And disable tracking new threads from CUDA
        monitor_disable_new_threads();

        // Walk the stream splay tree and close each trace.
        CloseAllStreams(stream_to_id_tree_root);
        stream_to_id_tree_root = NULL;

        CU_SAFE_CALL(cuEventDestroy(g_start_of_world_event));
        g_start_of_world_time = 0;
        // enable monitoring new threads
        monitor_enable_new_threads();
    }
    HPCRUN_ASYNC_UNBLOCK_SPIN_UNLOCK;

    CUresult ret = cuDriverFunctionPointer[CU_CTX_DESTROY].cuCtxDestroyReal(ctx);
    return ret;
}

CUresult cuEventCreate(CUevent * phEvent, unsigned int Flags) {

    TD_GET(is_thread_at_cuda_sync) = true;

    cudaError_t ret = cuDriverFunctionPointer[CU_EVENT_CREATE].cuEventCreateReal(phEvent, Flags);

    TD_GET(is_thread_at_cuda_sync) = false;

    return ret;

}

CUresult cuEventRecord(CUevent hEvent, CUstream hStream) {

    TD_GET(is_thread_at_cuda_sync) = true;

    cudaError_t ret = cuDriverFunctionPointer[CU_EVENT_RECORD].cuEventRecordReal(hEvent, hStream);

    TD_GET(is_thread_at_cuda_sync) = false;

    return ret;
}

CUresult cuMemcpyHtoDAsync(CUdeviceptr dstDevice, const void *srcHost, size_t ByteCount, CUstream hStream) {

    //TODO: tetsing , need to figure out correct implementation
    CreateStream0IfNot(hStream);

    uint32_t streamId = 0;
    event_list_node *event_node;

    streamId = SplayGetStreamId(stream_to_id_tree_root, hStream);
    // We cannot allow this thread to take samples since we will be holding a lock which is also needed in the async signal handler
    // let no other GPU work get ahead of me
    HPCRUN_ASYNC_BLOCK_SPIN_LOCK;
    // And disable tracking new threads from CUDA
    monitor_disable_new_threads();
    // Get CCT node (i.e., kernel launcher)
    ucontext_t context;
    getcontext(&context);
    cct_node_t *node = hpcrun_sample_callpath(&context, cpu_idle_metric_id, 0, 0 /*skipInner */ , 1 /*isSync */ );
    // Launcher CCT node will be 3 levels above in the loaded module ( Handler -> CUpti -> Cuda -> Launcher )
    // TODO: Get correct level .. 3 worked but not on S3D
    node = hpcrun_get_cct_node_n_levels_up_in_load_module(node, 0);

    /* FIXME: we are about to insert the cct into the stream 
     * we use the context above, then get teh backtrace, insert the backtrace
     * to the stream->epoch->csdata
     */
    cct_node_t *stream_cct = stream_backtrace2cct(g_stream_array[streamId].st, &context);

    // Create a new Cuda Event
    //cuptiGetStreamId(ctx, (CUstream) TD_GET(active_stream), &streamId);
    event_node = create_and_insert_event(streamId, node, stream_cct);
    // Insert the event in the stream
    //      assert(TD_GET(active_stream));
    fprintf(stderr, "\n start on stream = %p ", stream);

    CU_SAFE_CALL(cuDriverFunctionPointer[CU_EVENT_RECORD].cuEventRecordReal(event_node->event_start, hStream));

    CUresult ret = cuDriverFunctionPointer[CU_MEMCPY_H_TO_D_ASYNC].cuMemcpyHtoDAsyncReal(dstDevice, srcHost, ByteCount, hStream);

    CU_SAFE_CALL(cuDriverFunctionPointer[CU_EVENT_RECORD].cuEventRecordReal(event_node->event_end, hStream));

    // enable monitoring new threads
    monitor_enable_new_threads();
    // let other things be queued into GPU
    // safe to take async samples now
    HPCRUN_ASYNC_UNBLOCK_SPIN_UNLOCK;
    return ret;

}

CUresult cuMemcpyHtoD(CUdeviceptr dstDevice, const void *srcHost, size_t ByteCount) {

    SYNC_PROLOGUE(context, launcher_cct, syncStart, recorded_node);

    CUresult ret = cuDriverFunctionPointer[CU_MEMCPY_H_TO_D].cuMemcpyHtoDReal(dstDevice, srcHost, ByteCount);

    hpcrun_async_block();
    spinlock_lock(&g_gpu_lock);
    LeaveCudaSync(recorded_node, syncStart, ALL_STREAMS_MASK);
    spinlock_unlock(&g_gpu_lock);

    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t syncEnd = ((uint64_t) tv.tv_usec + (((uint64_t) tv.tv_sec) * 1000000));

    // this is both CPU and GPU idleness since one could have used cudaMemcpyAsync
    cct_metric_data_increment(cpu_idle_metric_id, launcher_cct, (cct_metric_data_t) {
                              .i = (syncEnd - syncStart)});
    cct_metric_data_increment(gpu_idle_metric_id, launcher_cct, (cct_metric_data_t) {
                              .i = (syncEnd - syncStart)});
    //idt += syncEnd - syncStart;
    //printf("\n gpu_idle_time = %lu .. %lu",syncEnd - syncStart,idt);

    hpcrun_async_unblock();

    TD_GET(is_thread_at_cuda_sync) = false;
    return ret;

}

CUresult cuMemcpyDtoHAsync(void *dstHost, CUdeviceptr srcDevice, size_t ByteCount, CUstream hStream) {
    //TODO: tetsing , need to figure out correct implementation
    CreateStream0IfNot(hStream);

    uint32_t streamId = 0;
    event_list_node *event_node;

    streamId = SplayGetStreamId(stream_to_id_tree_root, hStream);
    // We cannot allow this thread to take samples since we will be holding a lock which is also needed in the async signal handler
    // let no other GPU work get ahead of me
    HPCRUN_ASYNC_BLOCK_SPIN_LOCK;
    // And disable tracking new threads from CUDA
    monitor_disable_new_threads();
    // Get CCT node (i.e., kernel launcher)
    ucontext_t context;
    getcontext(&context);
    cct_node_t *node = hpcrun_sample_callpath(&context, cpu_idle_metric_id, 0, 0 /*skipInner */ , 1 /*isSync */ );
    // Launcher CCT node will be 3 levels above in the loaded module ( Handler -> CUpti -> Cuda -> Launcher )
    // TODO: Get correct level .. 3 worked but not on S3D
    node = hpcrun_get_cct_node_n_levels_up_in_load_module(node, 0);

    /* FIXME: we are about to insert the cct into the stream 
     * we use the context above, then get teh backtrace, insert the backtrace
     * to the stream->epoch->csdata
     */
    cct_node_t *stream_cct = stream_backtrace2cct(g_stream_array[streamId].st, &context);

    // Create a new Cuda Event
    //cuptiGetStreamId(ctx, (CUstream) TD_GET(active_stream), &streamId);
    event_node = create_and_insert_event(streamId, node, stream_cct);
    // Insert the event in the stream
    //      assert(TD_GET(active_stream));
    fprintf(stderr, "\n start on stream = %p ", stream);

    CU_SAFE_CALL(cuDriverFunctionPointer[CU_EVENT_RECORD].cuEventRecordReal(event_node->event_start, hStream));

    CUresult ret = cuDriverFunctionPointer[CU_MEMCPY_D_TO_H_ASYNC].cuMemcpyHtoDAsyncReal(dstHost, srcDevice, ByteCount, hStream);

    CU_SAFE_CALL(cuDriverFunctionPointer[CU_EVENT_RECORD].cuEventRecordReal(event_node->event_end, hStream));

    // enable monitoring new threads
    monitor_enable_new_threads();
    // let other things be queued into GPU
    // safe to take async samples now
    HPCRUN_ASYNC_UNBLOCK_SPIN_UNLOCK;

    return ret;
}

CUresult cuMemcpyDtoH(void *dstHost, CUdeviceptr srcDevice, size_t ByteCount) {

    SYNC_PROLOGUE(context, launcher_cct, syncStart, recorded_node);

    CUresult ret = cuDriverFunctionPointer[CU_MEMCPY_D_TO_H].cuMemcpyDtoHReal(dstHost, srcDevice, ByteCount);

    hpcrun_async_block();

    spinlock_lock(&g_gpu_lock);
    LeaveCudaSync(recorded_node, syncStart, ALL_STREAMS_MASK);
    spinlock_unlock(&g_gpu_lock);

    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t syncEnd = ((uint64_t) tv.tv_usec + (((uint64_t) tv.tv_sec) * 1000000));

    // this is both CPU and GPU idleness since one could have used cudaMemcpyAsync
    cct_metric_data_increment(cpu_idle_metric_id, launcher_cct, (cct_metric_data_t) {
                              .i = (syncEnd - syncStart)});
    cct_metric_data_increment(gpu_idle_metric_id, launcher_cct, (cct_metric_data_t) {
                              .i = (syncEnd - syncStart)});

    hpcrun_async_unblock();

    TD_GET(is_thread_at_cuda_sync) = false;
    return ret;
}

CUresult cuEventElapsedTime(float *pMilliseconds, CUevent hStart, CUevent hEnd) {

    TD_GET(is_thread_at_cuda_sync) = true;
    CUresult ret = cuDriverFunctionPointer[CU_EVENT_ELAPSED_TIME].cuEventElapsedTimeReal(pMilliseconds, hStart, hEnd);
    TD_GET(is_thread_at_cuda_sync) = false;
    return ret;
}

#endif                          // end of CUDA_RT_API

void gpu_blame_shift_itimer_signal_handler(cct_node_t * node, uint64_t cur_time_us, uint64_t metric_incr) {
    // If we are already in a cuda API, then we can't call cleanup_finished_events() since CUDA could have taken the same lock. Hence we just return.

    bool is_threads_at_sync = TD_GET(is_thread_at_cuda_sync);

    if (is_threads_at_sync)
        return;

    spinlock_lock(&g_gpu_lock);
    // when was the last cleanup done
    static uint64_t last_cleanup_time = 0;
    static uint32_t num_unfinshed_streams = 0;
    static stream_node *unfinished_event_list_head = 0;

    // if last cleanup happened (due to some other thread) less that metric_incr/2 time ago, then we will not do cleanup, but instead use its results
#if ENABLE_CLEANUP_OPTIMIZATION
    if (cur_time_us - last_cleanup_time < metric_incr * 0.5) {
        // reuse the last time recorded num_unfinshed_streams and unfinished_event_list_head
    } else
#endif                          //ENABLE_CLEANUP_OPTIMIZATION
        // do cleanup 
        last_cleanup_time = cur_time_us;
    num_unfinshed_streams = cleanup_finished_events();
    unfinished_event_list_head = g_unfinished_stream_list_head;

    if (num_unfinshed_streams) {
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
    } else {
        // GPU is idle

        // Increment gpu_ilde by metric_incr
        cct_metric_data_increment(gpu_idle_metric_id, node, (cct_metric_data_t) {
                                  .i = metric_incr}
        );

        //idt += metric_incr;
        //printf("\n gpu_idle_time = %lu, %lu",metric_incr, idt);

    }
    spinlock_unlock(&g_gpu_lock);
}

#endif
