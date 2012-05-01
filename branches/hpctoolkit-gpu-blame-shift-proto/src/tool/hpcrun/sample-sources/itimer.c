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
#define CUDA_RT_API

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
#include <lib/prof-lean/splay-macros.h>

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
#define NUM_GPU_DEVICES 2
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


#endif                          //ENABLE_CUDA



/******************************************************************************
 * local constants
 *****************************************************************************/

enum _local_const {
    ITIMER_EVENT = 0            // itimer has only 1 event
};

#ifdef ENABLE_CUDA
enum _cuda_const{
    KERNEL_START,
    KERNEL_END
};
#endif // end ENABLE_CUDA
/******************************************************************************
 * forward declarations 
 *****************************************************************************/

static int itimer_signal_handler(int sig, siginfo_t * siginfo, void *context);

#ifdef ENABLE_CUDA

typedef struct event_list_node {
    
#ifdef CUDA_RT_API    
    cudaEvent_t event_end;
    cudaEvent_t event_start;
#else // Driver API
    CUevent event_end;
    CUevent event_start;    
#endif
    
    uint64_t event_start_time; 
    uint64_t event_end_time; 
    cct_node_t *launcher_cct;
    uint32_t ref_count; 
    uint32_t stream_id;
    union {
    	struct event_list_node *next;
    	struct event_list_node *next_free_node;
    };
} event_list_node;



typedef struct active_kernel_node{
    uint64_t event_time; 
    bool  event_type; 
	uint32_t stream_id;
	union{
		cct_node_t * launcher_cct;  // present only in START nodes
		struct active_kernel_node * start_node;
	};
	union{
		struct active_kernel_node * next;
		struct active_kernel_node * next_free_node;
	};
	struct active_kernel_node * next_active_kernel;
	struct active_kernel_node * prev;
    
} active_kernel_node;	

/*
 *   array of g_stream_array
 *   this is for GPU purpose only
 */
typedef struct stream_node {
    struct event_list_node *latest_event_node;
    struct event_list_node *unfinished_event_node;
    struct stream_node *next_unfinished_stream;
} stream_node;


typedef struct stream_to_id_map{
#ifdef CUDA_RT_API    
	cudaStream_t stream;
#else // Driver API
    CUstream stream;
#endif    
	uint32_t id;
	struct stream_to_id_map * left;
	struct stream_to_id_map * right;
} stream_to_id_map;

stream_to_id_map stream_to_id[MAX_STREAMS];


#ifdef CUDA_RT_API

enum cudaRuntimeAPIIndex  {
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
    CUDA_EVENT_ELAPSED_TIME,
    CUDA_MAX_APIS
};

typedef struct cudaRuntimeFunctionPointer{
    union{
        cudaError_t(*generic) (void);
        cudaError_t(*cudaThreadSynchronizeReal) (void);
        cudaError_t(*cudaStreamSynchronizeReal) (cudaStream_t);
        cudaError_t(*cudaEventSynchronizeReal) (cudaEvent_t);
        cudaError_t(*cudaStreamWaitEventReal) (cudaStream_t, cudaEvent_t, unsigned int);
        cudaError_t(*cudaDeviceSynchronizeReal) (void);
        cudaError_t(*cudaConfigureCallReal) (dim3, dim3, size_t, cudaStream_t) ;
        cudaError_t(*cudaLaunchReal) (const char * );
        cudaError_t(*cudaStreamDestroyReal) (cudaStream_t );
        cudaError_t(*cudaStreamCreateReal) (cudaStream_t *);
        cudaError_t(*cudaMallocReal) (void **, size_t);
        cudaError_t(*cudaFreeReal) (void *);
        cudaError_t(*cudaMemcpyAsyncReal) (void * dst, const void * src, size_t count, enum cudaMemcpyKind kind, cudaStream_t);
        cudaError_t(*cudaMemcpyReal) (void * dst, const void * src, size_t count, enum cudaMemcpyKind kind);
	cudaError_t(*cudaEventElapsedTimeReal)(float * ms, cudaEvent_t start, cudaEvent_t end);
    };
    const char * functionName;
} cudaRuntimeFunctionPointer_t;

cudaRuntimeFunctionPointer_t  	cudaRuntimeFunctionPointer[] = {
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
    {0, "cudaEventElapsedTime"}
};

#else // Driver API
enum cuDriverAPIIndex  {
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

typedef struct cuDriverFunctionPointer{
    union{
        CUresult (*generic) (void);
        CUresult (*cuStreamCreateReal) (CUstream phStream, unsigned int Flags);
        CUresult (*cuStreamDestroyReal) (CUstream hStream);
        CUresult (*cuStreamSynchronizeReal) (CUstream hStream);
        CUresult (*cuEventSynchronizeReal) (CUevent event);
        CUresult (*cuLaunchGridAsyncReal) (CUfunction f, int grid_width, int grid_height, CUstream hStream);
        CUresult (*cuCtxDestroyReal)(CUcontext ctx);
        
        CUresult (*cuEventCreateReal) (CUevent * phEvent, unsigned int Flags);
        CUresult (*cuEventRecordReal) (CUevent hEvent, CUstream hStream);
        
        CUresult (*cuMemcpyHtoDAsyncReal) (CUdeviceptr dstDevice, const void * srcHost, size_t ByteCount, CUstream hStream); 
        CUresult (*cuMemcpyHtoDReal) (CUdeviceptr dstDevice, const void * srcHost, size_t ByteCount); 
        CUresult (*cuMemcpyDtoHAsyncReal) (void * dstHost, CUdeviceptr srcDevice, size_t ByteCount, CUstream hStream) ; 
        CUresult (*cuMemcpyDtoHReal) (void * dstHost, CUdeviceptr srcDevice, size_t ByteCount);                
	
	CUresult (*cuEventElapsedTimeReal)(float * pMilliseconds, CUevent hStart, CUevent hEnd);	

    };
    const char * functionName;
} cuDriverFunctionPointer_t;

cuDriverFunctionPointer_t  	cuDriverFunctionPointer[] = {
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
//Lock for wrapped synchronous calls
static spinlock_t g_cuda_lock = SPINLOCK_UNLOCKED;
static struct stream_to_id_map *  stream_to_id_tree_root = NULL;
// TODO.. Karthiks CCT fix needed, 32 since I assume max of 32 CPU threads
static uint32_t g_stream_id=32;
static uint32_t stream_to_id_index = 0;

// Lock for stream to id map
static spinlock_t g_stream_id_lock = SPINLOCK_UNLOCKED;

static uint64_t g_num_threads_at_sync;
static uint64_t g_active_threads;
static stream_node *g_stream_array;
static stream_node *g_unfinished_stream_list_head;
static event_list_node *g_free_event_nodes_head;
static event_list_node *g_finished_event_nodes_tail;
static active_kernel_node  *g_free_active_kernel_nodes_head;
static event_list_node  dummy_event_node = {.event_end = 0, .event_start = 0, .event_end_time = 0, .event_end_time = 0, .launcher_cct = 0};

static int wall_clock_metric_id;
static int cpu_idle_metric_id;
static int gpu_activity_time_metric_id;
static int cpu_idle_cause_metric_id;
static int gpu_idle_metric_id;
static int cpu_overlap_metric_id;
static int gpu_overlap_metric_id;

static uint64_t g_start_of_world_time;

#ifdef CUDA_RT_API
static cudaEvent_t g_start_of_world_event;
#else // Driver API
static CUevent g_start_of_world_event;
#endif

static bool g_stream0_not_initialized = true;


#endif                          //ENABLE_CUDA


// ******* METHOD DEFINITIONS ***********

#ifdef ENABLE_CUDA

#ifdef CUDA_RT_API
static struct stream_to_id_map * splay (struct stream_to_id_map * root, cudaStream_t  key){
#else
    static struct stream_to_id_map * splay (struct stream_to_id_map * root, CUstream key){
#endif
        REGULAR_SPLAY_TREE(stream_to_id_map, root, key, stream, left, right);
        return root;
    }
    
    
    
    static void CloseAllStreams(struct stream_to_id_map * root){
        
        if(!root)
            return;
        
        CloseAllStreams(root->left);
        CloseAllStreams(root->right);
        uint32_t streamId;
        streamId = root->id;
        gpu_trace_close(0, streamId);
    }
    
#ifdef CUDA_RT_API        
    static uint32_t SplayGetStreamId(struct stream_to_id_map * root, cudaStream_t  key){
#else // Driver API
        static uint32_t SplayGetStreamId(struct stream_to_id_map * root, cudaStream_t  key){
#endif    
            spinlock_lock(&g_stream_id_lock);
            REGULAR_SPLAY_TREE(stream_to_id_map, root, key, stream, left, right);
            stream_to_id_tree_root =  root;
            uint32_t ret = stream_to_id_tree_root->id;
            spinlock_unlock(&g_stream_id_lock);
            return ret;
            
        }
        
        
#ifdef CUDA_RT_API        
        
        static stream_to_id_map *
        splay_insert(cudaStream_t stream_ip)
        
#else // Driver API
        static stream_to_id_map *
        splay_insert(CUstream stream_ip)   
        
#endif    
        {
            
            spinlock_lock(&g_stream_id_lock);
            struct stream_to_id_map * node = & stream_to_id[stream_to_id_index++];
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
                    TMSG(MEMLEAK, "stream_to_id_tree_root  splay tree: unable to insert %p (already present)",
                         node->stream);
                    assert(0);
                }
            }
            stream_to_id_tree_root = node;
            spinlock_unlock(&g_stream_id_lock);
            return stream_to_id_tree_root;
        }
        
        
#ifdef CUDA_RT_API        
        static struct stream_to_id_map  * splay_delete(cudaStream_t stream)
#else
        static struct stream_to_id_map  * splay_delete(CUstream stream)
#endif
        {
            struct stream_to_id_map  *result = NULL;
            
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
            stream_to_id_tree_root =  stream_to_id_tree_root->left;
            spinlock_unlock(&g_stream_id_lock);
            return result;
        }
        
        
        
        
        static inline event_list_node  *  EnterCudaSync(uint64_t  * syncStart){
            /// caller does HPCRUN_ASYNC_BLOCK_SPIN_LOCK;
            
            // Cleanup events so that when I goto wait anybody in the queue will be the ones I have not seen and finished after my timer started.
            cleanup_finished_events();
            
            struct timeval tv;
            gettimeofday(&tv, NULL);
            *syncStart  = ((uint64_t)tv.tv_usec 
                           + (((uint64_t)tv.tv_sec) * 1000000));
            
            //fprintf(stderr, "\n Start of world = %lu", g_start_of_world_time);
            //fprintf(stderr, "\n Sync start at  = %lu", *syncStart);
            event_list_node  * recorded_node = g_finished_event_nodes_tail;
            if(g_finished_event_nodes_tail != &dummy_event_node)
                g_finished_event_nodes_tail->ref_count++;	
            
            atomic_add_i64(&g_num_threads_at_sync, 1L);
            // caller does  HPCRUN_ASYNC_UNBLOCK_SPIN_UNLOCK;
            return recorded_node;
        }
        
        
        static uint64_t  attribute_shared_blame_on_kernels(event_list_node * recorded_node, uint64_t recorded_time, const uint32_t stream_mask){
            
            // if recorded_node is not dummy_event_node decrement its ref count
            if(recorded_node != & dummy_event_node)
                recorded_node->ref_count--;
            
            uint32_t num_active_kernels = 0;
            active_kernel_node * sorted_active_kernels_begin = NULL;
            
            // Traverse all nodes, inserting them in a sorted list if their end times were past the recorded time
            // If their start times were before the recorded, just record them as recorded_time
            
            event_list_node * cur = recorded_node->next, *prev = recorded_node;
            while( cur != & dummy_event_node) {
                // if the node's refcount is already zero, then free it and we dont care about it
                if( cur->ref_count == 0) {
                    prev->next = cur->next;
                    event_list_node * to_free = cur;
                    cur = cur->next;
                    ADD_TO_FREE_EVENTS_LIST(to_free);
                    continue;
                }
                
                cur->ref_count--;
                
                if ( (cur->event_end_time <= recorded_time) ||
                    (cur->stream_id != (cur->stream_id & stream_mask))) {
                    if( cur->ref_count == 0) {
                        prev->next = cur->next;
                        event_list_node * to_free = cur;
                        cur = cur->next;
                        ADD_TO_FREE_EVENTS_LIST(to_free);
                    } else {
                        prev = cur;
                        cur = cur->next;
                    }
                    continue;
                }
                
                // Add start and end times in a sorted list (insertion sort)
                
                active_kernel_node * start_active_kernel_node;
                active_kernel_node * end_active_kernel_node;
                GET_NEW_ACTIVE_KERNEL_NODE(start_active_kernel_node);
                GET_NEW_ACTIVE_KERNEL_NODE(end_active_kernel_node);
                
                if(cur->event_start_time < recorded_time) {
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
                if ( start_active_kernel_node->event_time == end_active_kernel_node->event_time)
                {
                    ADD_TO_FREE_ACTIVE_KERNEL_NODE_LIST(start_active_kernel_node);
                    ADD_TO_FREE_ACTIVE_KERNEL_NODE_LIST(end_active_kernel_node);
                    if( cur->ref_count == 0) {
                        prev->next = cur->next;
                        event_list_node * to_free = cur;
                        cur = cur->next;
                        ADD_TO_FREE_EVENTS_LIST(to_free);
                    } else {
                        prev = cur;
                        cur = cur->next;
                    }
                    continue;
                }
                assert ( start_active_kernel_node->event_time < end_active_kernel_node->event_time);
                
                if (sorted_active_kernels_begin == NULL){
                    // First entry
                    start_active_kernel_node->next = end_active_kernel_node;
                    start_active_kernel_node->prev = end_active_kernel_node;
                    end_active_kernel_node->prev = start_active_kernel_node;
                    end_active_kernel_node->next = start_active_kernel_node;
                    sorted_active_kernels_begin = start_active_kernel_node;
                } else {
                    // There are atlest 2 entries
                    
                    // current points to the last node interms of time	
                    active_kernel_node * current = sorted_active_kernels_begin->prev;
                    bool change_head = 1;
                    do {
                        if ( end_active_kernel_node->event_time > current->event_time   ) {
                            change_head = 0;
                            break;
                        } 
                        current = current->prev;
                    } while(current != sorted_active_kernels_begin->prev);
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
                        if ( start_active_kernel_node->event_time > current->event_time   ) {
                            change_head = 0;
                            break;
                        } 
                        current= current->prev;
                    } while(current != sorted_active_kernels_begin->prev);
                    start_active_kernel_node->next = current->next;
                    start_active_kernel_node->prev = current;
                    current->next->prev = start_active_kernel_node;
                    current->next = start_active_kernel_node;
                    if (change_head) {
                        sorted_active_kernels_begin = start_active_kernel_node;
                    }	
                }
                
                if( cur->ref_count == 0) {
                    prev->next = cur->next;
                    event_list_node * to_free = cur;
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
            if(sorted_active_kernels_begin){
                
                // attach a dummy tail 
                active_kernel_node * dummy_kernel_node;
                GET_NEW_ACTIVE_KERNEL_NODE(dummy_kernel_node);
                sorted_active_kernels_begin->prev->next = dummy_kernel_node;
                dummy_kernel_node->prev = sorted_active_kernels_begin->prev;
                sorted_active_kernels_begin->prev = dummy_kernel_node;
                dummy_kernel_node->next = sorted_active_kernels_begin;
                
                
                active_kernel_node * current = sorted_active_kernels_begin;
                uint64_t last_time = recorded_time;
                do{
                    uint64_t new_time = current->event_time;
                    
                    assert(new_time >= last_time);
                    assert(current != dummy_kernel_node && "should never process dummy_kernel_node");
                    
                    if ( num_active_kernels && (new_time > last_time ) ){
                        //blame all 
                        active_kernel_node * blame_node = current->prev;
                        do{
                            assert ( blame_node->event_type == KERNEL_START );
                            
                            cct_metric_data_increment(cpu_idle_cause_metric_id, blame_node->launcher_cct, (cct_metric_data_t) {
                                .r = (new_time - last_time)  * 1.0 / num_active_kernels } );
                            blame_node = blame_node->prev;
                        }while(blame_node != sorted_active_kernels_begin->prev);
                    }
                    
                    last_time = new_time;
                    
                    if( current->event_type == KERNEL_START ) {
                        num_active_kernels++;			
                        current = current->next;
                    } else {
                        last_kernel_end_time = new_time;
                        current->start_node->prev->next = current->start_node->next;
                        current->start_node->next->prev = current->start_node->prev;
                        if(current->start_node == sorted_active_kernels_begin)
                            sorted_active_kernels_begin = current->start_node->next;	
                        ADD_TO_FREE_ACTIVE_KERNEL_NODE_LIST((current->start_node));
                        
#if 0 // Not a plausible case
                        // If I am the last one then Just free and break;
                        if ( current->next == current){
                            ADD_TO_FREE_ACTIVE_KERNEL_NODE_LIST(current);
                            break;
                        }
#endif
                        current->prev->next = current->next;
                        current->next->prev = current->prev;
                        if(current == sorted_active_kernels_begin)
                            sorted_active_kernels_begin = current->next;	
                        num_active_kernels--;
                        active_kernel_node * to_free = current;
                        current = current->next;
                        ADD_TO_FREE_ACTIVE_KERNEL_NODE_LIST(to_free);
                        
                    }
                    
                    
                    
                }while(current != sorted_active_kernels_begin->prev);	 
                // Free up the dummy node
                ADD_TO_FREE_ACTIVE_KERNEL_NODE_LIST(dummy_kernel_node);
                
            }
            return last_kernel_end_time;
            
        }
        
        static inline uint64_t LeaveCudaSync(event_list_node  * recorded_node, uint64_t syncStart, const uint32_t stream_mask){
            //caller does	HPCRUN_ASYNC_BLOCK_SPIN_LOCK;
            
            // Cleanup events so that when I goto wait anybody in the queue will be the ones I have not seen and finished after my timer started.
            cleanup_finished_events();
            
            uint64_t last_kernel_end_time = attribute_shared_blame_on_kernels(recorded_node, syncStart, stream_mask); 
            atomic_add_i64(&g_num_threads_at_sync, -1L);
            return last_kernel_end_time;
            //caller does	HPCRUN_ASYNC_UNBLOCK_SPIN_UNLOCK;
        }
        
        static uint32_t cleanup_finished_events()
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
                        float elapsedTime; // in millisec with 0.5 microsec resolution as per CUDA
                        
                        
                        //FIX ME: deleting Elapsed time to handle context destruction.... 
                        //static uint64_t deleteMeTime = 0;
#ifdef CUDA_RT_API            
                        CUDA_SAFE_CALL(cudaRuntimeFunctionPointer[CUDA_EVENT_ELAPSED_TIME].cudaEventElapsedTimeReal(&elapsedTime, g_start_of_world_event, current_event->event_start));
#else
                        CU_SAFE_CALL(cuDriverFunctionPointer[CU_EVENT_ELAPSED_TIME].cuEventElapsedTimeReal(&elapsedTime, g_start_of_world_event, current_event->event_start));
                        
#endif                
                        assert(elapsedTime > 0 );
                        
                        uint64_t micro_time_start = ((uint64_t)elapsedTime) * 1000 + g_start_of_world_time;
                        
#if 0
                        fprintf(stderr, "\n started at  :  %lu", micro_time_start);
#endif
                        // TODO : START TRACE WITH  with IDLE MARKER .. This should go away after trace viewer changes
                        cct_bundle_t *bundle = &(TD_GET(epoch)->csdata);
                        cct_node_t *idl = hpcrun_cct_bundle_get_idle_node(bundle);
                        hpcrun_cct_persistent_id_trace_mutate(idl);
                        
                        gpu_trace_append_with_time(0, GET_STREAM_ID(cur_stream), hpcrun_cct_persistent_id(idl), micro_time_start - 1);
                        
                        gpu_trace_append_with_time( /*gpu_number */ 0, GET_STREAM_ID(cur_stream), hpcrun_cct_persistent_id(launcher_cct),  micro_time_start);
                        // record end time
                        // TODO : WE JUST NEED TO PUT IDLE MARKER
                        
#ifdef CUDA_RT_API                            
                        CUDA_SAFE_CALL(cudaRuntimeFunctionPointer[CUDA_EVENT_ELAPSED_TIME].cudaEventElapsedTimeReal(&elapsedTime, g_start_of_world_event, current_event->event_end));
#else
                        CU_SAFE_CALL(cuDriverFunctionPointer[CU_EVENT_ELAPSED_TIME].cuEventElapsedTimeReal(&elapsedTime, g_start_of_world_event, current_event->event_end));
#endif
                        assert(elapsedTime > 0 );
                        uint64_t micro_time_end = ((uint64_t)elapsedTime) * 1000 + g_start_of_world_time;
#if 0
                        fprintf(stderr, "\n Ended  at  :  %lu", micro_time_end);
#endif
                        gpu_trace_append_with_time( /*gpu_number */ 0, GET_STREAM_ID(cur_stream), hpcrun_cct_persistent_id(launcher_cct),  micro_time_end);
                        
                        
                        // TODO : WE JUST NEED TO PUT IDLE MARKER
                        gpu_trace_append_with_time( /*gpu_number */ 0, GET_STREAM_ID(cur_stream), hpcrun_cct_persistent_id(idl),  micro_time_end + 1);
#if 0
                        
                        err_cuda = cudaEventElapsedTime(&elapsedTime, current_event->event_start, current_event->event_end);
                        //fprintf(stderr, "\n Length of Event :  %f", elapsedTime);
                        
#endif
                        
                        
                        
                        
                        assert(micro_time_start <= micro_time_end );
                        
                        // Add the kernel execution time to the gpu_activity_time_metric_id
                        cct_metric_data_increment(gpu_activity_time_metric_id,current_event->launcher_cct, (cct_metric_data_t) {
                            .i = (micro_time_end - micro_time_start) });
                        
                        
                        
                        event_list_node *deferred_node = current_event;
                        current_event = current_event->next;
                        // Add to_free to fre list 
                        
                        
                        if( g_num_threads_at_sync ) {
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
        
        
        static event_list_node *create_and_insert_event(int stream_id, cct_node_t * launcher_cct)
        {
            
#ifdef CUDA_RT_API    
            cudaError_t err;
#else // Driver API
            CUresult err;
#endif
            
            event_list_node *event_node;
            if (g_free_event_nodes_head) {
                // get from free list
                event_node = g_free_event_nodes_head;
                // Destroy those old used ones 
                assert(event_node->event_start);
                
#ifdef CUDA_RT_API            
                CUDA_SAFE_CALL(cudaEventDestroy(event_node->event_start));
#else  // Driver API
                ////////TODO: FIX ME CANT DELETE IF STREAM IS DELETED       err =  cuEventDestroy(event_node->event_start);
                ///CHECK_CU_ERROR(err, "cudaEventDestroy");
#endif        
                
                assert(event_node->event_end);
                
                
#ifdef CUDA_RT_API            
                CUDA_SAFE_CALL(cudaEventDestroy(event_node->event_end));
#else  // Driver API
                ///// TODO: FIX ME: CANT DELETE AFTER STREAM IS DELETED        err = cuEventDestroy(event_node->event_end);
                ///    	CHECK_CU_ERROR(err, "cudaEventDestroy");
#endif
                g_free_event_nodes_head = g_free_event_nodes_head->next_free_node;
            } else {
                // allocate new node
                event_node = (event_list_node *) hpcrun_malloc(sizeof(event_list_node));
            }
            //cudaError_t err =  cudaEventCreateWithFlags(&(event_node->event_end),cudaEventDisableTiming);
            
#ifdef CUDA_RT_API    
            CUDA_SAFE_CALL(cudaEventCreate(&(event_node->event_start)));
#else
            CU_SAFE_CALL(cuDriverFunctionPointer[CU_EVENT_CREATE].cuEventCreateReal(&(event_node->event_start), CU_EVENT_DEFAULT));
#endif    
            
            
#ifdef CUDA_RT_API    
            CUDA_SAFE_CALL(cudaEventCreate(&(event_node->event_end)));
#else
            CU_SAFE_CALL(cuDriverFunctionPointer[CU_EVENT_CREATE].cuEventCreateReal(&(event_node->event_end), CU_EVENT_DEFAULT));
#endif    
            
            
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
        static void PopulateEntryPointesToWrappedCudaRuntimeCalls(){
            void *handle;
            char *error;
            
            handle = dlopen("libcudart.so", RTLD_LAZY);
            if (!handle) {
                fputs(dlerror(), stderr);
                exit(1);
            }
            
            for(int i = 0 ; i < CUDA_MAX_APIS; i++){
                cudaRuntimeFunctionPointer[i].generic = dlsym(handle, cudaRuntimeFunctionPointer[i].functionName);
                if ((error = dlerror()) != NULL) {
                    fprintf(stderr, "%s\n", error);
                    exit(1);
                }
            }
        }
#else
        static void PopulateEntryPointesToWrappedCuDriverCalls(){
            void *handle;
            char *error;
            
            handle = dlopen("libcuda.so", RTLD_LAZY);
            if (!handle) {
                fputs(dlerror(), stderr);
                exit(1);
            }
            
            for(int i = 0 ; i < CU_MAX_APIS; i++){
                cuDriverFunctionPointer[i].generic = dlsym(handle, cuDriverFunctionPointer[i].functionName);
                if ((error = dlerror()) != NULL) {
                    fprintf(stderr, "%s\n", error);
                    exit(1);
                }
            }
        }
#endif
        
        static void PopulateEntryPointesToWrappedCalls(){
#ifdef CUDA_RT_API    
            PopulateEntryPointesToWrappedCudaRuntimeCalls();
#else
            PopulateEntryPointesToWrappedCuDriverCalls();
#endif
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
#if 0
            err = cuInit(0);
            // TODO: gracefully handle absense of CUDA
            CHECK_CU_ERROR(err, "cuInit");
            
            err = cuDeviceGetCount(&deviceCount);
            CHECK_CU_ERROR(err, "cuDeviceGetCount");
#else
            cudaGetDeviceCount(&deviceCount);
#endif
            
            // TODO: gracefully handle absense of device
            if (deviceCount == 0) {
                printf("There is no device supporting CUDA.\n");
                exit(-1);
            }
            // TODO: check device capabilities
            //
            // NON CUPTI
#if 0
            cuptiErr = cuptiSubscribe(&subscriber, (CUpti_CallbackFunc) EventInsertionCallback, 0);
            //TODO: gracefully handle failure
            CHECK_CUPTI_ERROR(cuptiErr, "cuptiSubscribe");
#endif
            
            
            // Enable runtime APIs
            // TODO: enable just the ones you need
            //cuptiErr = cuptiEnableDomain(1 /*enable */ , subscriber, CUPTI_CB_DOMAIN_RUNTIME_API);
            //CHECK_CUPTI_ERROR(cuptiErr, "cuptiEnableDomain");
            // NON CUPTI
#if 0
            cuptiErr = cuptiEnableCallback(1 /*enable */ , subscriber, CUPTI_CB_DOMAIN_RUNTIME_API, CUPTI_RUNTIME_TRACE_CBID_cudaLaunch_v3020);
            CHECK_CUPTI_ERROR(cuptiErr, "cuptiEnableCallback");
            cuptiErr = cuptiEnableCallback(1 /*enable */ , subscriber, CUPTI_CB_DOMAIN_RUNTIME_API, CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyAsync_v3020);
            CHECK_CUPTI_ERROR(cuptiErr, "cuptiEnableCallback");
#endif
            // NON CUPTI
#if 0
            cuptiErr = cuptiEnableCallback(1 /*enable */ , subscriber, CUPTI_CB_DOMAIN_RUNTIME_API, CUPTI_RUNTIME_TRACE_CBID_cudaConfigureCall_v3020);
            CHECK_CUPTI_ERROR(cuptiErr, "cuptiEnableCallback");
#endif
            
            //TODO:anything else ?  Enable resource APIs
            //cuptiErr = cuptiEnableDomain(1 /*enable */ , subscriber, CUPTI_CB_DOMAIN_RESOURCE);
            //CHECK_CUPTI_ERROR(cuptiErr, "cuptiEnableDomain");
#if 0
            cuptiErr = cuptiEnableCallback(1 /*enable */ , subscriber, CUPTI_CB_DOMAIN_RESOURCE, CUPTI_CBID_RESOURCE_STREAM_CREATED);
            CHECK_CUPTI_ERROR(cuptiErr, "cuptiEnableCallback");
            cuptiErr = cuptiEnableCallback(1 /*enable */ , subscriber, CUPTI_CB_DOMAIN_RESOURCE, CUPTI_CBID_RESOURCE_STREAM_DESTROY_STARTING);
            CHECK_CUPTI_ERROR(cuptiErr, "cuptiEnableCallback");
#endif
            
#if 0
            // Enable synchronization  APIs
            cuptiErr = cuptiEnableDomain(1 /*enable */ , subscriber, CUPTI_CB_DOMAIN_SYNCHRONIZE);
            CHECK_CUPTI_ERROR(cuptiErr, "cuptiEnableDomain");
#endif
            
            
            
            
            
            
            
            // Initialize CUDA events data structure 
            g_stream_array = (stream_node *) hpcrun_malloc(sizeof(stream_node) * MAX_STREAMS);
            memset(g_stream_array, 0, sizeof(stream_node) * MAX_STREAMS);
            g_unfinished_stream_list_head = NULL;
            g_finished_event_nodes_tail = & dummy_event_node;
            dummy_event_node.next = g_finished_event_nodes_tail; 
#if 0
            // Always have stream 0 ready
            splay_insert(0);
            
            
            HPCRUN_ASYNC_BLOCK_SPIN_LOCK;
            if (g_start_of_world_time == 0) {
                // And disable tracking new threads from CUDA
                monitor_disable_new_threads();
                
                // Initialize and Record an event to indicate the start of this stream.
                // No need to wait for it since we query only the events posted after this and this will be complete when the latter posted ones are complete.
                cudaError_t err = cudaEventCreate(&g_start_of_world_event);
                CHECK_CU_ERROR(err, "cudaEventCreate");
                
                // record time
                
                struct timeval tv;
                gettimeofday(&tv, NULL);
                g_start_of_world_time = ((uint64_t)tv.tv_usec
                                         + (((uint64_t)tv.tv_sec) * 1000000));
                
                // record in stream 0       
                err = cudaEventRecord(g_start_of_world_event, 0);
                CHECK_CU_ERROR(err, "cudaEventRecord");
                // enable monitoring new threads
                monitor_enable_new_threads();
            }
            HPCRUN_ASYNC_UNBLOCK_SPIN_UNLOCK;
#endif
            
            
            PopulateEntryPointesToWrappedCalls();
            
            
#endif                          // ENABLE_CUDA
            
            
        }
        
        static void METHOD_FN(thread_init)
        {
#ifdef ENABLE_CUDA
            atomic_add_i64(&g_active_threads, 1L);
#else
            TMSG(ITIMER_CTL, "thread init (no action needed)");
#endif
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
            
            
#ifdef ENABLE_CUDA
            if (fetch_and_add(&g_active_threads, -1L) == 1L) {
                SYNCHRONOUS_CLEANUP;
                // Walk the stream splay tree and close each trace.
                CloseAllStreams(stream_to_id_tree_root);
                stream_to_id_tree_root = NULL;
            }
#endif
            
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
            
            
#if 0
#ifdef ENABLE_CUDA
            if (!g_stream0_not_initialized) {
                fprintf(stderr, "\n MAY BE BROKEN FOR MULTITHREADED");
                uint32_t streamId;
                streamId = SplayGetStreamId(stream_to_id_tree_root, 0);
                SYNCHRONOUS_CLEANUP;
                gpu_trace_close(0, streamId);
            }
#endif
#endif
            
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
            wall_clock_metric_id = metric_id;
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
            
            TMSG(ITIMER_CTL, "setting metric itimer period = %ld", sample_period);
            hpcrun_set_metric_info_and_period(metric_id, "WALLCLOCK (us)", MetricFlags_ValFmt_Int, sample_period);
            hpcrun_set_metric_info_and_period(cpu_idle_metric_id, "CPU_IDLE", MetricFlags_ValFmt_Real, 1);
            hpcrun_set_metric_info_and_period(gpu_idle_metric_id, "GPU_IDLE_CAUSE", MetricFlags_ValFmt_Real, 1);
            hpcrun_set_metric_info_and_period(cpu_idle_cause_metric_id, "CPU_IDLE_CAUSE", MetricFlags_ValFmt_Real, 1);
            hpcrun_set_metric_info_and_period(cpu_overlap_metric_id, "OVERLAPPED_CPU", MetricFlags_ValFmt_Real, 1);
            hpcrun_set_metric_info_and_period(gpu_overlap_metric_id, "OVERLAPPED_GPU", MetricFlags_ValFmt_Real, 1);
            hpcrun_set_metric_info_and_period(gpu_activity_time_metric_id, "GPU_ACTIVITY_TIME", MetricFlags_ValFmt_Int, 1);
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
        
        
#ifdef CUDA_RT_API    
        void CreateStream0IfNot(cudaStream_t stream){
#else    
            void CreateStream0IfNot(CUstream stream){
#endif        // if stream is 0 and if it is not created, then create one:
                HPCRUN_ASYNC_BLOCK_SPIN_LOCK;
                if((uint64_t) stream == 0 && g_stream0_not_initialized) { 
                    uint32_t new_streamId;
                    new_streamId = splay_insert(0)->id;
                    fprintf(stderr,"\n Stream id = %d", new_streamId);
                    if (g_start_of_world_time == 0) {
                        // And disable tracking new threads from CUDA
                        monitor_disable_new_threads();
                        
                        
                        // Initialize and Record an event to indicate the start of this stream.
                        // No need to wait for it since we query only the events posted after this and this will be complete when the latter posted ones are complete.
                        
#ifdef CUDA_RT_API
                        CUDA_SAFE_CALL(cudaEventCreate(&g_start_of_world_event));
#else
                        CU_SAFE_CALL(cuDriverFunctionPointer[CU_EVENT_CREATE].cuEventCreateReal(&g_start_of_world_event, CU_EVENT_DEFAULT));
#endif        
                        
                        // record time
                        
                        struct timeval tv;
                        gettimeofday(&tv, NULL);
                        g_start_of_world_time = ((uint64_t)tv.tv_usec
                                                 + (((uint64_t)tv.tv_sec) * 1000000));
                        
                        // record in stream 0       
#ifdef CUDA_RT_API
                        CUDA_SAFE_CALL(cudaEventRecord(g_start_of_world_event, 0));
#else
                        CU_SAFE_CALL(cuDriverFunctionPointer[CU_EVENT_RECORD].cuEventRecordReal(g_start_of_world_event, 0));                
#endif                
                        
                        // enable monitoring new threads
                        monitor_enable_new_threads();
                    }
                    
                    
                    gpu_trace_open(0, new_streamId);
                    cct_bundle_t *bundle = &(TD_GET(epoch)->csdata);
                    cct_node_t *idl = hpcrun_cct_bundle_get_idle_node(bundle);
                    hpcrun_cct_persistent_id_trace_mutate(idl);
                    gpu_trace_append(0, new_streamId, hpcrun_cct_persistent_id(idl));
                    g_stream0_not_initialized = false;
                }
                HPCRUN_ASYNC_UNBLOCK_SPIN_UNLOCK;
                
            }
            
            
            
#ifdef CUDA_RT_API
            cudaError_t cudaThreadSynchronize(void) {
                hpcrun_async_block();
                
                // Get CCT node (i.e., kernel launcher)
                ucontext_t context;
                getcontext(&context);
                cct_node_t * launcher_cct = hpcrun_sample_callpath(&context, cpu_idle_metric_id, 0 , 0 /*skipInner */ , 1 /*isSync */ );
                // Enter into the trace
                hpcrun_cct_persistent_id_trace_mutate(launcher_cct);
                trace_append(hpcrun_cct_persistent_id(launcher_cct));
                
                
                
                TD_GET(is_thread_at_cuda_sync) = true;
                
                spinlock_lock(&g_gpu_lock);
                uint64_t syncStart;
                event_list_node  *  recorded_node = EnterCudaSync(& syncStart);
                spinlock_unlock(&g_gpu_lock);
                
                
                cudaError_t ret = cudaRuntimeFunctionPointer[CUDA_THREAD_SYNCHRONIZE].cudaThreadSynchronizeReal();
                
                spinlock_lock(&g_gpu_lock);
                LeaveCudaSync(recorded_node,syncStart,ALL_STREAMS_MASK);
                spinlock_unlock(&g_gpu_lock);
                
                
                
                struct timeval tv;
                gettimeofday(&tv, NULL);
                uint64_t syncEnd  = ((uint64_t)tv.tv_usec + (((uint64_t)tv.tv_sec) * 1000000));
                cct_metric_data_increment(cpu_idle_metric_id, launcher_cct, (cct_metric_data_t) {.r = (syncEnd - syncStart)});
                // Since we were asyn blocked increase the time by syncEnd - syncStart
                cct_metric_data_increment(wall_clock_metric_id, launcher_cct, (cct_metric_data_t) {.i = (syncEnd - syncStart)});
                
                
                // Enter into the trace
                trace_append(hpcrun_cct_persistent_id(launcher_cct));
                
                hpcrun_async_unblock();
                
                TD_GET(is_thread_at_cuda_sync) = false;
                return ret;
            }
            
            cudaError_t cudaStreamSynchronize(cudaStream_t stream)
            {
                fprintf(stderr,"\n %s","cudaStreamSynchronize");
                hpcrun_async_block();
                
                
                // Get CCT node (i.e., kernel launcher)
                ucontext_t context;
                getcontext(&context);
                cct_node_t * launcher_cct = hpcrun_sample_callpath(&context, cpu_idle_metric_id, 0 , 0 /*skipInner */ , 1 /*isSync */ );
                // Enter into the trace
                hpcrun_cct_persistent_id_trace_mutate(launcher_cct);
                trace_append(hpcrun_cct_persistent_id(launcher_cct));
                
                
                
                
                TD_GET(is_thread_at_cuda_sync) = true;
                
                spinlock_lock(&g_gpu_lock);
                uint64_t syncStart;
                event_list_node  *  recorded_node = EnterCudaSync(& syncStart);
                spinlock_unlock(&g_gpu_lock);
                
                cudaError_t ret = cudaRuntimeFunctionPointer[CUDA_STREAM_SYNCHRONIZE].cudaStreamSynchronizeReal(stream);
                //TODO:get stream
                //CUcontext ctx;
                //cuCtxGetCurrent(&ctx); 
                uint32_t streamId;
                //cuptiGetStreamId(ctx,stream,&streamId);
                //TODO: delete me
                streamId = SplayGetStreamId(stream_to_id_tree_root, stream);
                
                spinlock_lock(&g_gpu_lock);
                LeaveCudaSync(recorded_node,syncStart, streamId);
                spinlock_unlock(&g_gpu_lock);
                
                struct timeval tv;
                gettimeofday(&tv, NULL);
                uint64_t syncEnd  = ((uint64_t)tv.tv_usec + (((uint64_t)tv.tv_sec) * 1000000));
                
                cct_metric_data_increment(cpu_idle_metric_id, launcher_cct, (cct_metric_data_t) {.r = (syncEnd - syncStart)});
                // Since we were asyn blocked increase the time by syncEnd - syncStart
                cct_metric_data_increment(wall_clock_metric_id, launcher_cct, (cct_metric_data_t) {.i = (syncEnd - syncStart)});
                
                // Enter into the trace
                trace_append(hpcrun_cct_persistent_id(launcher_cct));
                hpcrun_async_unblock();
                TD_GET(is_thread_at_cuda_sync) = false;
                return ret;
            }
            
            
            
            
            cudaError_t cudaEventSynchronize(cudaEvent_t event)
            {
                fprintf(stderr,"\n %s","cudaEventSynchronize");
                hpcrun_async_block();
                // Get CCT node (i.e., kernel launcher)
                ucontext_t context;
                getcontext(&context);
                cct_node_t * launcher_cct = hpcrun_sample_callpath(&context, cpu_idle_metric_id, 0 , 0 /*skipInner */ , 1 /*isSync */ );
                // Enter into the trace
                hpcrun_cct_persistent_id_trace_mutate(launcher_cct);
                trace_append(hpcrun_cct_persistent_id(launcher_cct));
                
                
                
                
                
                
                TD_GET(is_thread_at_cuda_sync) = true;
                
                
                spinlock_lock(&g_gpu_lock);
                uint64_t syncStart;
                event_list_node  *  recorded_node = EnterCudaSync(& syncStart);
                spinlock_unlock(&g_gpu_lock);
                
                cudaError_t ret = cudaRuntimeFunctionPointer[CUDA_EVENT_SYNCHRONIZE].cudaEventSynchronizeReal(event);
                
                spinlock_lock(&g_gpu_lock);
                LeaveCudaSync(recorded_node,syncStart,ALL_STREAMS_MASK);
                spinlock_unlock(&g_gpu_lock);
                
                struct timeval tv;
                gettimeofday(&tv, NULL);
                uint64_t syncEnd  = ((uint64_t)tv.tv_usec + (((uint64_t)tv.tv_sec) * 1000000));
                
                cct_metric_data_increment(cpu_idle_metric_id, launcher_cct, (cct_metric_data_t) {.r = (syncEnd - syncStart)});
                // Since we were asyn blocked increase the time by syncEnd - syncStart
                cct_metric_data_increment(wall_clock_metric_id, launcher_cct, (cct_metric_data_t) {.i = (syncEnd - syncStart)});
                
                
                // Enter into the trace
                trace_append(hpcrun_cct_persistent_id(launcher_cct));
                
                hpcrun_async_unblock();
                TD_GET(is_thread_at_cuda_sync) = false;
                return ret;
            }
            
            cudaError_t cudaStreamWaitEvent(cudaStream_t stream, cudaEvent_t event, unsigned int flags)
            {
                fprintf(stderr,"\n %s","cudaStreamWaitEvent");
                hpcrun_async_block();
                
                // Get CCT node (i.e., kernel launcher)
                ucontext_t context;
                getcontext(&context);
                cct_node_t * launcher_cct = hpcrun_sample_callpath(&context, cpu_idle_metric_id, 0 , 0 /*skipInner */ , 1 /*isSync */ );
                // Enter into the trace
                hpcrun_cct_persistent_id_trace_mutate(launcher_cct);
                trace_append(hpcrun_cct_persistent_id(launcher_cct));
                
                
                
                TD_GET(is_thread_at_cuda_sync) = true;
                
                spinlock_lock(&g_gpu_lock);
                uint64_t syncStart;
                event_list_node  *  recorded_node = EnterCudaSync(& syncStart);
                spinlock_unlock(&g_gpu_lock);
                
                cudaError_t ret = cudaRuntimeFunctionPointer[CUDA_STREAM_WAIT_EVENT].cudaStreamWaitEventReal(stream, event, flags);
                
                uint32_t streamId;
                streamId = SplayGetStreamId(stream_to_id_tree_root, stream);
                
                
                spinlock_lock(&g_gpu_lock);
                LeaveCudaSync(recorded_node,syncStart,streamId);
                spinlock_unlock(&g_gpu_lock);
                struct timeval tv;
                gettimeofday(&tv, NULL);
                uint64_t syncEnd  = ((uint64_t)tv.tv_usec + (((uint64_t)tv.tv_sec) * 1000000));
                
                cct_metric_data_increment(cpu_idle_metric_id, launcher_cct, (cct_metric_data_t) {.r = (syncEnd - syncStart)});
                // Since we were asyn blocked increase the time by syncEnd - syncStart
                cct_metric_data_increment(wall_clock_metric_id, launcher_cct, (cct_metric_data_t) {.i = (syncEnd - syncStart)});
                
                // Enter into the trace
                trace_append(hpcrun_cct_persistent_id(launcher_cct));
                
                hpcrun_async_unblock();
                
                TD_GET(is_thread_at_cuda_sync) = false;
                return ret;
            }
            
            
            
            
            cudaError_t cudaDeviceSynchronize(void)
            {
                fprintf(stderr,"\n %s","cudaDeviceSynchronize");
                hpcrun_async_block();
                // Get CCT node (i.e., kernel launcher)
                ucontext_t context;
                getcontext(&context);
                cct_node_t * launcher_cct = hpcrun_sample_callpath(&context, cpu_idle_metric_id, 0 , 0 /*skipInner */ , 1 /*isSync */ );
                // Enter into the trace
                hpcrun_cct_persistent_id_trace_mutate(launcher_cct);
                trace_append(hpcrun_cct_persistent_id(launcher_cct));
                
                
                
                TD_GET(is_thread_at_cuda_sync) = true;
                
                spinlock_lock(&g_gpu_lock);
                uint64_t syncStart;
                event_list_node  *  recorded_node = EnterCudaSync(& syncStart);
                spinlock_unlock(&g_gpu_lock);
                
                cudaError_t ret = cudaRuntimeFunctionPointer[CUDA_DEVICE_SYNCHRONIZE].cudaDeviceSynchronizeReal();
                
                spinlock_lock(&g_gpu_lock);
                LeaveCudaSync(recorded_node,syncStart,ALL_STREAMS_MASK);
                spinlock_unlock(&g_gpu_lock);
                
                struct timeval tv;
                gettimeofday(&tv, NULL);
                uint64_t syncEnd  = ((uint64_t)tv.tv_usec + (((uint64_t)tv.tv_sec) * 1000000)); 
                
                cct_metric_data_increment(cpu_idle_metric_id, launcher_cct, (cct_metric_data_t) {.r = (syncEnd - syncStart)});
                // Since we were asyn blocked increase the time by syncEnd - syncStart
                cct_metric_data_increment(wall_clock_metric_id, launcher_cct, (cct_metric_data_t) {.i = (syncEnd - syncStart)});
                
                // Enter into the trace
                trace_append(hpcrun_cct_persistent_id(launcher_cct));
                
                hpcrun_async_unblock();
                
                TD_GET(is_thread_at_cuda_sync) = false;
                return ret;
            }
            
            cudaError_t cudaConfigureCall(dim3 grid, dim3 block, size_t mem, cudaStream_t stream)
            {
                
#if 0
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
                
                // Create a new Cuda Event
                //cuptiGetStreamId(ctx, (CUstream) TD_GET(active_stream), &streamId);
                event_node = create_and_insert_event(streamId, node);
                // Insert the event in the stream
                cudaError_t err = cudaEventRecord(event_node->event_start, stream);
                CHECK_CU_ERROR(err, "cudaEventRecord");
                TD_GET(event_node) = event_node;
#endif
                
                fprintf(stderr,"\n %s","cudaConfigCall");
                hpcrun_async_block();
                
                cudaError_t ret = cudaRuntimeFunctionPointer[CUDA_CONFIGURE_CALL].cudaConfigureCallReal(grid, block, mem, stream);
                
                TD_GET(active_stream) = (uint64_t) stream;
                fprintf(stderr,"\n stream = %p ", TD_GET(active_stream)); 
                hpcrun_async_unblock();
                
                //TODO: tetsing , need to figure out correct implementation
                CreateStream0IfNot(stream);
                
                
                return ret;
            }
            
            
            cudaError_t cudaLaunch(const char * entry)
            {
                fprintf(stderr,"\n %s","cudaLaunch");
                //printf("\n %s on %lu","cudaLaunch",TD_GET(active_stream));
                
#if 1
                uint32_t streamId = 0;
                event_list_node *event_node;
                
                //TODO:get stream
                //CUcontext ctx;
                // cuCtxGetCurrent(&ctx); 
                //cuptiGetStreamId(ctx,(CUstream) TD_GET(active_stream),&streamId);
                //TODO: delete me
                //	assert(TD_GET(active_stream));
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
                
                
                // Create a new Cuda Event
                //cuptiGetStreamId(ctx, (CUstream) TD_GET(active_stream), &streamId);
                event_node = create_and_insert_event(streamId, node);
                // Insert the event in the stream
                //	assert(TD_GET(active_stream));
                fprintf(stderr,"\n start on stream = %p ", TD_GET(active_stream));
                CUDA_SAFE_CALL(cudaEventRecord(event_node->event_start, (cudaStream_t) TD_GET(active_stream)));
                
#else
                
                event_list_node *event_node = TD_GET(event_node);
#endif    
                cudaError_t ret = cudaRuntimeFunctionPointer[CUDA_LAUNCH].cudaLaunchReal(entry);
#if 1
                fprintf(stderr,"\n end  on stream = %p ", TD_GET(active_stream));
                CUDA_SAFE_CALL(cudaEventRecord(event_node->event_end, (cudaStream_t) TD_GET(active_stream)));
                // enable monitoring new threads
                monitor_enable_new_threads();
                // let other things be queued into GPU
                // safe to take async samples now
                HPCRUN_ASYNC_UNBLOCK_SPIN_UNLOCK;
#endif
                return ret;
            }
            
            
            
            
            cudaError_t cudaStreamDestroy(cudaStream_t  stream)
            {
                
                SYNCHRONOUS_CLEANUP;
                fprintf(stderr,"\n %s","cudaStreamDestroy");
                hpcrun_async_block();
                
                uint32_t streamId;
                //CUcontext ctx;
                //cuCtxGetCurrent(&ctx);
                //cuptiGetStreamId(ctx,stream,&streamId);
                //TODO: delete me
                streamId = SplayGetStreamId(stream_to_id_tree_root, stream);
                
                gpu_trace_close(0, streamId);
                
                
                cudaError_t ret = cudaRuntimeFunctionPointer[CUDA_STREAM_DESTROY].cudaStreamDestroyReal(stream);
                
                // Delete splay tree entry
                splay_delete(stream);
                hpcrun_async_unblock();
                return ret;
                
            }
            
            
            cudaError_t cudaStreamCreate(cudaStream_t * stream)
            {
                fprintf(stderr,"\n %s","cudaStreamCreate");
                
                cudaError_t ret = cudaRuntimeFunctionPointer[CUDA_STREAM_CREATE].cudaStreamCreateReal(stream);
                
                
                uint32_t new_streamId;           
                //CUcontext ctx;
                //cuCtxGetCurrent(&ctx);
                //cuptiGetStreamId(ctx,*stream,&new_streamId);
                //TODO: delete me
                new_streamId = splay_insert(*stream)->id;
                fprintf(stderr,"\n Stream id = %d", new_streamId);
                
                
                
                HPCRUN_ASYNC_BLOCK_SPIN_LOCK;
                if (g_start_of_world_time == 0) {
                    // And disable tracking new threads from CUDA
                    monitor_disable_new_threads();
                    
                    
                    // Initialize and Record an event to indicate the start of this stream.
                    // No need to wait for it since we query only the events posted after this and this will be complete when the latter posted ones are complete.
                    CUDA_SAFE_CALL(cudaEventCreate(&g_start_of_world_event));
                    
                    // record time
                    
                    struct timeval tv;
                    gettimeofday(&tv, NULL);
                    g_start_of_world_time = ((uint64_t)tv.tv_usec
                                             + (((uint64_t)tv.tv_sec) * 1000000));
                    
                    // record in stream 0       
                    CUDA_SAFE_CALL(cudaEventRecord(g_start_of_world_event, 0));
                    
                    // enable monitoring new threads
                    monitor_enable_new_threads();
                }
                HPCRUN_ASYNC_UNBLOCK_SPIN_UNLOCK;
                
                gpu_trace_open(0, new_streamId);
                cct_bundle_t *bundle = &(TD_GET(epoch)->csdata);
                cct_node_t *idl = hpcrun_cct_bundle_get_idle_node(bundle);
                hpcrun_cct_persistent_id_trace_mutate(idl);
                gpu_trace_append(0, new_streamId, hpcrun_cct_persistent_id(idl));
                
                
                
                
                return ret;
            }
            
            
            
            
            
            
            cudaError_t cudaMalloc(void ** devPtr, size_t size)
            {
                
                fprintf(stderr,"\n %s","cudaMalloc");
                hpcrun_async_block();
                // Get CCT node (i.e., kernel launcher)
                ucontext_t context;
                getcontext(&context);
                cct_node_t * launcher_cct = hpcrun_sample_callpath(&context, cpu_idle_metric_id, 0 , 0 /*skipInner */ , 1 /*isSync */ );
                // Enter into the trace
                hpcrun_cct_persistent_id_trace_mutate(launcher_cct);
                trace_append(hpcrun_cct_persistent_id(launcher_cct));
                
                TD_GET(is_thread_at_cuda_sync) = true;
                
                
                spinlock_lock(&g_gpu_lock);
                uint64_t syncStart;
                event_list_node  *  recorded_node = EnterCudaSync(& syncStart);
                spinlock_unlock(&g_gpu_lock);
                
                cudaError_t ret = cudaRuntimeFunctionPointer[CUDA_MALLOC].cudaMallocReal(devPtr,size);
                
                spinlock_lock(&g_gpu_lock);
                uint64_t last_kernel_end_time = LeaveCudaSync(recorded_node,syncStart,ALL_STREAMS_MASK);
                spinlock_unlock(&g_gpu_lock);
                
                struct timeval tv;
                gettimeofday(&tv, NULL);
                uint64_t syncEnd  = ((uint64_t)tv.tv_usec + (((uint64_t)tv.tv_sec) * 1000000)); 
                
                
                // if the end of last kernel was sooner than the end of cudaMalloc(), the remaining time was wasted in Mallocing ... which is GPU idle time.
                // Hence 
                // CPU_idle_time = LastKernelEndTime - syncStart
                // GPU_idle_time = syncEnd - LastKernelEndTime
                // Sadly there can be some skew :(, hence let us take the non-zero value only.
                uint64_t cpu_idle_time = last_kernel_end_time == 0 ? 0: last_kernel_end_time  - syncStart;
                assert(cpu_idle_time >= 0); // TODO: can become -ve if last_kernel_end_time  < syncStart 
                uint64_t gpu_idle_time = last_kernel_end_time == 0 ? syncEnd - syncStart : syncEnd - last_kernel_end_time;
                assert(gpu_idle_time >= 0); // TODO: can become -ve if isyncEnd < last_kernel_end_time
                cct_metric_data_increment(cpu_idle_metric_id, launcher_cct, (cct_metric_data_t) {.r = (cpu_idle_time)});
                cct_metric_data_increment(gpu_idle_metric_id, launcher_cct, (cct_metric_data_t) {.r = (gpu_idle_time)});
                
                // Since we were asyn blocked increase the time by syncEnd - syncStart
                cct_metric_data_increment(wall_clock_metric_id, launcher_cct, (cct_metric_data_t) {.i = (syncEnd - syncStart)});
                
                
                
                
                // Enter into the trace
                trace_append(hpcrun_cct_persistent_id(launcher_cct));
                
                hpcrun_async_unblock();
                TD_GET(is_thread_at_cuda_sync) = false;
                
                return ret;
                
                
            }
            
            
            
            cudaError_t cudaFree(void * devPtr)
            { 
                
                
                fprintf(stderr,"\n %s","cudaFree");
                hpcrun_async_block();
                // Get CCT node (i.e., kernel launcher)
                ucontext_t context;
                getcontext(&context);
                cct_node_t * launcher_cct = hpcrun_sample_callpath(&context, cpu_idle_metric_id, 0 , 0 /*skipInner */ , 1 /*isSync */ );
                // Enter into the trace
                hpcrun_cct_persistent_id_trace_mutate(launcher_cct);
                trace_append(hpcrun_cct_persistent_id(launcher_cct));
                
                
                
                
                
                TD_GET(is_thread_at_cuda_sync) = true;
                
                
                spinlock_lock(&g_gpu_lock);
                uint64_t syncStart;
                event_list_node  *  recorded_node = EnterCudaSync(& syncStart);
                spinlock_unlock(&g_gpu_lock);
                
                cudaError_t ret = cudaRuntimeFunctionPointer[CUDA_FREE].cudaFreeReal(devPtr);
                
                spinlock_lock(&g_gpu_lock);
                uint64_t last_kernel_end_time = LeaveCudaSync(recorded_node,syncStart,ALL_STREAMS_MASK);
                spinlock_unlock(&g_gpu_lock);
                
                struct timeval tv;
                gettimeofday(&tv, NULL);
                uint64_t syncEnd  = ((uint64_t)tv.tv_usec + (((uint64_t)tv.tv_sec) * 1000000)); 
                
                
                // if the end of last kernel was sooner than the end of cudaFree(), the remaining time was wasted in Freeing ... which is GPU idle time.
                // Hence 
                // CPU_idle_time = LastKernelEndTime - syncStart
                // GPU_idle_time = syncEnd - LastKernelEndTime
                // Sadly there can be some skew :(, hence let us take the non-zero value only.
                uint64_t cpu_idle_time = last_kernel_end_time == 0 ? 0: last_kernel_end_time  - syncStart;
                assert(cpu_idle_time >= 0); // TODO: can become -ve if last_kernel_end_time  < syncStart 
                uint64_t gpu_idle_time = last_kernel_end_time == 0 ? syncEnd - syncStart : syncEnd - last_kernel_end_time;
                assert(gpu_idle_time >= 0); // TODO: can become -ve if isyncEnd < last_kernel_end_time
                
                cct_metric_data_increment(cpu_idle_metric_id, launcher_cct, (cct_metric_data_t) {.r = (cpu_idle_time)});
                cct_metric_data_increment(gpu_idle_metric_id, launcher_cct, (cct_metric_data_t) {.r = (gpu_idle_time)});
                
                // Since we were asyn blocked increase the time by syncEnd - syncStart
                cct_metric_data_increment(wall_clock_metric_id, launcher_cct, (cct_metric_data_t) {.i = (syncEnd - syncStart)});
                
                // Enter into the trace
                trace_append(hpcrun_cct_persistent_id(launcher_cct));
                
                hpcrun_async_unblock();
                
                TD_GET(is_thread_at_cuda_sync) = false;
                return ret;
                
            } 
            
            cudaError_t cudaMemcpyAsync(void * dst, const void * src, size_t count, enum cudaMemcpyKind kind, cudaStream_t stream)
            {
                
                
#if 1
                //TODO: tetsing , need to figure out correct implementation
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
                
                // Create a new Cuda Event
                //cuptiGetStreamId(ctx, (CUstream) TD_GET(active_stream), &streamId);
                event_node = create_and_insert_event(streamId, node);
                // Insert the event in the stream
                //      assert(TD_GET(active_stream));
                fprintf(stderr,"\n start on stream = %p ", stream);
                CUDA_SAFE_CALL(cudaEventRecord(event_node->event_start, stream));
#else
                
                event_list_node *event_node = TD_GET(event_node);
#endif
                cudaError_t ret = cudaRuntimeFunctionPointer[CUDA_MEMCPY_ASYNC].cudaMemcpyAsyncReal(dst,src,count,kind,stream);
#if 1
                fprintf(stderr,"\n end  on stream = %p ", stream);
                CUDA_SAFE_CALL(cudaEventRecord(event_node->event_end, stream));
                // enable monitoring new threads
                monitor_enable_new_threads();
                // let other things be queued into GPU
                // safe to take async samples now
                HPCRUN_ASYNC_UNBLOCK_SPIN_UNLOCK;
#endif
                
                return ret;
            }
            
            
            cudaError_t cudaMemcpy(void * dst, const void * src, size_t count, enum cudaMemcpyKind kind)
            {
                
                hpcrun_async_block();
                
                // Get CCT node (i.e., kernel launcher)
                ucontext_t context;
                getcontext(&context);
                cct_node_t * launcher_cct = hpcrun_sample_callpath(&context, cpu_idle_metric_id, 0 , 0 /*skipInner */ , 1 /*isSync */ );
                // Enter into the trace
                hpcrun_cct_persistent_id_trace_mutate(launcher_cct);
                trace_append(hpcrun_cct_persistent_id(launcher_cct));
                
                
                TD_GET(is_thread_at_cuda_sync) = true;
                
                spinlock_lock(&g_gpu_lock);
                uint64_t syncStart;
                event_list_node  *  recorded_node = EnterCudaSync(& syncStart);
                spinlock_unlock(&g_gpu_lock);
                
                cudaError_t ret = cudaRuntimeFunctionPointer[CUDA_MEMCPY].cudaMemcpyReal(dst,src,count,kind);
                
                spinlock_lock(&g_gpu_lock);
                LeaveCudaSync(recorded_node,syncStart,ALL_STREAMS_MASK);
                spinlock_unlock(&g_gpu_lock);
                
                struct timeval tv;
                gettimeofday(&tv, NULL);
                uint64_t syncEnd  = ((uint64_t)tv.tv_usec + (((uint64_t)tv.tv_sec) * 1000000)); 
                
                // this is both CPU and GPU idleness since one could have used cudaMemcpyAsync
                cct_metric_data_increment(cpu_idle_metric_id, launcher_cct, (cct_metric_data_t) {.r = (syncEnd - syncStart)});
                cct_metric_data_increment(gpu_idle_metric_id, launcher_cct, (cct_metric_data_t) {.r = (syncEnd - syncStart)});
                
                // Since we were asyn blocked increase the time by syncEnd - syncStart
                cct_metric_data_increment(wall_clock_metric_id, launcher_cct, (cct_metric_data_t) {.i = (syncEnd - syncStart)});
                
                
                // Enter into the trace
                trace_append(hpcrun_cct_persistent_id(launcher_cct));
                
                hpcrun_async_unblock();
                
                TD_GET(is_thread_at_cuda_sync) = false;
                return ret;
                
            }

	    cudaError_t cudaEventElapsedTime(float * ms, cudaEvent_t start, cudaEvent_t end){
		hpcrun_async_block();
                cudaError_t ret = cudaRuntimeFunctionPointer[CUDA_EVENT_ELAPSED_TIME].cudaEventElapsedTimeReal(ms, start, end);
		hpcrun_async_unblock();
                return ret;
	    }

#else // Driver APIs
            
            
            
            CUresult cuStreamSynchronize(CUstream stream)
            {
                hpcrun_async_block();
                
                
                // Get CCT node (i.e., kernel launcher)
                ucontext_t context;
                getcontext(&context);
                cct_node_t * launcher_cct = hpcrun_sample_callpath(&context, cpu_idle_metric_id, 0 , 0 /*skipInner */ , 1 /*isSync */ );
                // Enter into the trace
                hpcrun_cct_persistent_id_trace_mutate(launcher_cct);
                trace_append(hpcrun_cct_persistent_id(launcher_cct));
                
                
                
                
                TD_GET(is_thread_at_cuda_sync) = true;
                
                spinlock_lock(&g_gpu_lock);
                uint64_t syncStart;
                event_list_node  *  recorded_node = EnterCudaSync(& syncStart);
                spinlock_unlock(&g_gpu_lock);
                
                CUresult ret = cuDriverFunctionPointer[CU_STREAM_SYNCHRONIZE].cuStreamSynchronizeReal(stream);
                //TODO:get stream
                //CUcontext ctx;
                //cuCtxGetCurrent(&ctx); 
                uint32_t streamId;
                //cuptiGetStreamId(ctx,stream,&streamId);
                //TODO: delete me
                streamId = SplayGetStreamId(stream_to_id_tree_root, stream);
                
                spinlock_lock(&g_gpu_lock);
                LeaveCudaSync(recorded_node,syncStart, streamId);
                spinlock_unlock(&g_gpu_lock);
                
                struct timeval tv;
                gettimeofday(&tv, NULL);
                uint64_t syncEnd  = ((uint64_t)tv.tv_usec + (((uint64_t)tv.tv_sec) * 1000000));
                
                cct_metric_data_increment(cpu_idle_metric_id, launcher_cct, (cct_metric_data_t) {.r = (syncEnd - syncStart)});
                // Since we were asyn blocked increase the time by syncEnd - syncStart
                cct_metric_data_increment(wall_clock_metric_id, launcher_cct, (cct_metric_data_t) {.i = (syncEnd - syncStart)});
                
                // Enter into the trace
                trace_append(hpcrun_cct_persistent_id(launcher_cct));
                hpcrun_async_unblock();
                TD_GET(is_thread_at_cuda_sync) = false;
                return ret;
            }
            
            
            CUresult cuEventSynchronize(CUevent event)
            {
                hpcrun_async_block();
                // Get CCT node (i.e., kernel launcher)
                ucontext_t context;
                getcontext(&context);
                cct_node_t * launcher_cct = hpcrun_sample_callpath(&context, cpu_idle_metric_id, 0 , 0 /*skipInner */ , 1 /*isSync */ );
                // Enter into the trace
                hpcrun_cct_persistent_id_trace_mutate(launcher_cct);
                trace_append(hpcrun_cct_persistent_id(launcher_cct));
                
                
                
                
                
                
                TD_GET(is_thread_at_cuda_sync) = true;
                
                
                spinlock_lock(&g_gpu_lock);
                uint64_t syncStart;
                event_list_node  *  recorded_node = EnterCudaSync(& syncStart);
                spinlock_unlock(&g_gpu_lock);
                
                CUresult ret = cuDriverFunctionPointer[CU_EVENT_SYNCHRONIZE].cuEventSynchronizeReal(event);
                
                spinlock_lock(&g_gpu_lock);
                LeaveCudaSync(recorded_node,syncStart,ALL_STREAMS_MASK);
                spinlock_unlock(&g_gpu_lock);
                
                struct timeval tv;
                gettimeofday(&tv, NULL);
                uint64_t syncEnd  = ((uint64_t)tv.tv_usec + (((uint64_t)tv.tv_sec) * 1000000));
                
                cct_metric_data_increment(cpu_idle_metric_id, launcher_cct, (cct_metric_data_t) {.r = (syncEnd - syncStart)});
                // Since we were asyn blocked increase the time by syncEnd - syncStart
                cct_metric_data_increment(wall_clock_metric_id, launcher_cct, (cct_metric_data_t) {.i = (syncEnd - syncStart)});
                
                
                // Enter into the trace
                trace_append(hpcrun_cct_persistent_id(launcher_cct));
                
                hpcrun_async_unblock();
                TD_GET(is_thread_at_cuda_sync) = false;
                return ret;
            }
            
            
            CUresult cuLaunchGridAsync (CUfunction f, int grid_width, int grid_height, CUstream hStream) {
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
                
                
                // Create a new Cuda Event
                //cuptiGetStreamId(ctx, (CUstream) TD_GET(active_stream), &streamId);
                event_node = create_and_insert_event(streamId, node);
                // Insert the event in the stream
                //	assert(TD_GET(active_stream));
                
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
            
            
            
            
            CUresult cuStreamDestroy(CUstream  stream)
            {
                
                SYNCHRONOUS_CLEANUP;
                hpcrun_async_block();
                
                uint32_t streamId;
                streamId = SplayGetStreamId(stream_to_id_tree_root, stream);
                
                gpu_trace_close(0, streamId);
                
                
                cudaError_t ret = cuDriverFunctionPointer[CU_STREAM_DESTROY].cuStreamDestroyReal(stream);
                
                // Delete splay tree entry
                splay_delete(stream);
                hpcrun_async_unblock();
                return ret;
                
            }
            
            
            
            
            CUresult cuStreamCreate (CUstream * phStream, unsigned int Flags){
                
                CUresult ret = cuDriverFunctionPointer[CU_STREAM_CREATE].cuStreamCreateReal(phStream, Flags);
                
                
                uint32_t new_streamId;           
                new_streamId = splay_insert((cudaStream_t) *phStream)->id;
                fprintf(stderr,"\n Stream id = %d", new_streamId);
                
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
                    g_start_of_world_time = ((uint64_t)tv.tv_usec
                                             + (((uint64_t)tv.tv_sec) * 1000000));
                    
                    // record in stream 0    
                    
                    CU_SAFE_CALL(cuDriverFunctionPointer[CU_EVENT_RECORD].cuEventRecordReal(g_start_of_world_event, 0));                
                    
                    
                    // enable monitoring new threads
                    monitor_enable_new_threads();
                }
                HPCRUN_ASYNC_UNBLOCK_SPIN_UNLOCK;
                
                
                gpu_trace_open(0, new_streamId);
                cct_bundle_t *bundle = &(TD_GET(epoch)->csdata);
                cct_node_t *idl = hpcrun_cct_bundle_get_idle_node(bundle);
                hpcrun_cct_persistent_id_trace_mutate(idl);
                gpu_trace_append(0, new_streamId, hpcrun_cct_persistent_id(idl));
                
                
                
                //cuptiGetStreamId(cbRDInfo->context, cbRDInfo->resourceHandle.stream, &new_streamId);
                //SYNCHRONOUS_CLEANUP;
                //gpu_trace_close(0, new_streamId);
                
                
                return ret;
                
            }
            
            CUresult cuCtxDestroy(CUcontext ctx)
            {
                
                
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
            
            
            CUresult cuEventCreate (CUevent * phEvent, unsigned int Flags){
                hpcrun_async_block();            
                cudaError_t ret = cuDriverFunctionPointer[CU_EVENT_CREATE].cuEventCreateReal(phEvent, Flags);
                hpcrun_async_unblock();
                return ret;
                
            }
            
            CUresult cuEventRecord (CUevent hEvent, CUstream hStream){
                hpcrun_async_block();            
                cudaError_t ret = cuDriverFunctionPointer[CU_EVENT_RECORD].cuEventRecordReal(hEvent, hStream);
                hpcrun_async_unblock();
                return ret;            
            }
            
            
            
            CUresult cuMemcpyHtoDAsync(CUdeviceptr dstDevice, const void * srcHost, size_t ByteCount, CUstream hStream) {
                
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
                
                // Create a new Cuda Event
                //cuptiGetStreamId(ctx, (CUstream) TD_GET(active_stream), &streamId);
                event_node = create_and_insert_event(streamId, node);
                // Insert the event in the stream
                //      assert(TD_GET(active_stream));
                fprintf(stderr,"\n start on stream = %p ", stream);
                
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
            
            
            CUresult cuMemcpyHtoD (CUdeviceptr dstDevice, const void * srcHost, size_t ByteCount) {
                
                
                hpcrun_async_block();
                
                // Get CCT node (i.e., kernel launcher)
                ucontext_t context;
                getcontext(&context);
                cct_node_t * launcher_cct = hpcrun_sample_callpath(&context, cpu_idle_metric_id, 0 , 0 /*skipInner */ , 1 /*isSync */ );
                // Enter into the trace
                hpcrun_cct_persistent_id_trace_mutate(launcher_cct);
                trace_append(hpcrun_cct_persistent_id(launcher_cct));
                
                
                TD_GET(is_thread_at_cuda_sync) = true;
                
                spinlock_lock(&g_gpu_lock);
                uint64_t syncStart;
                event_list_node  *  recorded_node = EnterCudaSync(& syncStart);
                spinlock_unlock(&g_gpu_lock);
                
                CUresult ret = cuDriverFunctionPointer[CU_MEMCPY_H_TO_D].cuMemcpyHtoDReal(dstDevice, srcHost, ByteCount);
                
                spinlock_lock(&g_gpu_lock);
                LeaveCudaSync(recorded_node,syncStart,ALL_STREAMS_MASK);
                spinlock_unlock(&g_gpu_lock);
                
                struct timeval tv;
                gettimeofday(&tv, NULL);
                uint64_t syncEnd  = ((uint64_t)tv.tv_usec + (((uint64_t)tv.tv_sec) * 1000000)); 
                
                // this is both CPU and GPU idleness since one could have used cudaMemcpyAsync
                cct_metric_data_increment(cpu_idle_metric_id, launcher_cct, (cct_metric_data_t) {.r = (syncEnd - syncStart)});
                cct_metric_data_increment(gpu_idle_metric_id, launcher_cct, (cct_metric_data_t) {.r = (syncEnd - syncStart)});
                
                // Since we were asyn blocked increase the time by syncEnd - syncStart
                cct_metric_data_increment(wall_clock_metric_id, launcher_cct, (cct_metric_data_t) {.i = (syncEnd - syncStart)});
                
                
                // Enter into the trace
                trace_append(hpcrun_cct_persistent_id(launcher_cct));
                
                hpcrun_async_unblock();
                
                TD_GET(is_thread_at_cuda_sync) = false;
                return ret;
                
                
            }
            
            CUresult cuMemcpyDtoHAsync(void * dstHost, CUdeviceptr srcDevice, size_t ByteCount, CUstream hStream){
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
                
                // Create a new Cuda Event
                //cuptiGetStreamId(ctx, (CUstream) TD_GET(active_stream), &streamId);
                event_node = create_and_insert_event(streamId, node);
                // Insert the event in the stream
                //      assert(TD_GET(active_stream));
                fprintf(stderr,"\n start on stream = %p ", stream);
                
                
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
            
            CUresult cuMemcpyDtoH (void * dstHost, CUdeviceptr srcDevice, size_t ByteCount) {
                hpcrun_async_block();
                
                // Get CCT node (i.e., kernel launcher)
                ucontext_t context;
                getcontext(&context);
                cct_node_t * launcher_cct = hpcrun_sample_callpath(&context, cpu_idle_metric_id, 0 , 0 /*skipInner */ , 1 /*isSync */ );
                // Enter into the trace
                hpcrun_cct_persistent_id_trace_mutate(launcher_cct);
                trace_append(hpcrun_cct_persistent_id(launcher_cct));
                
                
                TD_GET(is_thread_at_cuda_sync) = true;
                
                spinlock_lock(&g_gpu_lock);
                uint64_t syncStart;
                event_list_node  *  recorded_node = EnterCudaSync(& syncStart);
                spinlock_unlock(&g_gpu_lock);
                
                CUresult ret = cuDriverFunctionPointer[CU_MEMCPY_D_TO_H].cuMemcpyDtoHReal(dstHost, srcDevice, ByteCount);
                
                spinlock_lock(&g_gpu_lock);
                LeaveCudaSync(recorded_node,syncStart,ALL_STREAMS_MASK);
                spinlock_unlock(&g_gpu_lock);
                
                struct timeval tv;
                gettimeofday(&tv, NULL);
                uint64_t syncEnd  = ((uint64_t)tv.tv_usec + (((uint64_t)tv.tv_sec) * 1000000)); 
                
                // this is both CPU and GPU idleness since one could have used cudaMemcpyAsync
                cct_metric_data_increment(cpu_idle_metric_id, launcher_cct, (cct_metric_data_t) {.r = (syncEnd - syncStart)});
                cct_metric_data_increment(gpu_idle_metric_id, launcher_cct, (cct_metric_data_t) {.r = (syncEnd - syncStart)});
                
                // Since we were asyn blocked increase the time by syncEnd - syncStart
                cct_metric_data_increment(wall_clock_metric_id, launcher_cct, (cct_metric_data_t) {.i = (syncEnd - syncStart)});
                
                
                // Enter into the trace
                trace_append(hpcrun_cct_persistent_id(launcher_cct));
                
                hpcrun_async_unblock();
                
                TD_GET(is_thread_at_cuda_sync) = false;
                return ret;            
            }

	    CUresult cuEventElapsedTime(float * pMilliseconds, CUevent hStart, CUevent hEnd) {
		
                hpcrun_async_block();
		CUresult ret = cuDriverFunctionPointer[CU_EVENT_ELAPSED_TIME].cuEventElapsedTimeReal(pMilliseconds, hStart, hEnd);
                hpcrun_async_unblock();
		return ret;
	    }
            
            
#endif // end of CUDA_RT_API
            
            
            
            void CUPTIAPI EventInsertionCallback(void *userdata, CUpti_CallbackDomain domain, CUpti_CallbackId cbid, const void *cbData)
            {
                
                //CUptiResult cuptiErr;
                cudaError_t err;
                uint32_t streamId = 0;
                event_list_node *event_node;
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
                                    // Get stream id
                                    cudaConfigureCall_v3020_params *params = (cudaConfigureCall_v3020_params *) cbInfo->functionParams;
                                    TD_GET(active_stream) = (uint64_t) params->stream;
                                }
                            }
                                break;
                                
                            case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyAsync_v3020:
                                if (cbInfo->callbackSite == CUPTI_API_ENTER) {
                                    // We cannot allow this thread to take samples since we will be holding a lock which is also needed in the async signal handler
                                    // let no other GPU work get ahead of me
                                    HPCRUN_ASYNC_BLOCK_SPIN_LOCK;
                                    // And disable tracking new threads from CUDA
                                    monitor_disable_new_threads();
                                    //TODO: this should change, we should create streams for mem copies.
                                    cudaMemcpyAsync_v3020_params *params = (cudaMemcpyAsync_v3020_params *) cbInfo->functionParams;
                                    TD_GET(active_stream) = (uint64_t) params->stream;
                                    // Get CCT node (i.e., kernel launcher)
                                    ucontext_t context;
                                    getcontext(&context);
                                    //cct_node_t *node = hpcrun_gen_thread_ctxt(&context);
                                    cct_node_t *node = hpcrun_sample_callpath(&context, cpu_idle_metric_id, 0, 0 /*skipInner */ , 1 /*isSync */ );
                                    // Launcher CCT node will be 3 levels above in the loaded module ( Handler -> CUpti -> Cuda -> Launcher )
                                    // TODO: Get correct level .. 3 worked but not on S3D
                                    node = hpcrun_get_cct_node_n_levels_up_in_load_module(node, 0);
                                    
                                    // Create a new Cuda Event
                                    cuptiGetStreamId(cbInfo->context, (CUstream) TD_GET(active_stream), &streamId);
                                    event_node = create_and_insert_event(streamId, node);
                                    TD_GET(event_node) = event_node;
                                    // Insert the event in the stream
                                    err = cudaEventRecord(event_node->event_start, (cudaStream_t) TD_GET(active_stream));
                                    CHECK_CU_ERROR(err, "cudaEventRecord");
                                } else {
                                    event_node = TD_GET(event_node);
                                    err = cudaEventRecord(event_node->event_end, (cudaStream_t) TD_GET(active_stream));
                                    CHECK_CU_ERROR(err, "cudaEventRecord");
                                    // enable monitoring new threads
                                    monitor_enable_new_threads();
                                    // let other things be queued into GPU
                                    // safe to take async samples now
                                    HPCRUN_ASYNC_UNBLOCK_SPIN_UNLOCK;
                                }
                                break;
                                // case CUPTI_RUNTIME_TRACE_CBID_cudaMemsetAsync_v3020:
                                // TODO : Trace memcpy NON async
                                //case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy_v3020:
                                //case CUPTI_RUNTIME_TRACE_CBID_cudaMemset_v3020:
                                /*	DANGER!!!    default:	
                                 if (cbInfo->callbackSite == CUPTI_API_ENTER) {
                                 hpcrun_async_block();
                                 } else {
                                 hpcrun_async_unblock();
                                 }
                                 break; */
                            case CUPTI_RUNTIME_TRACE_CBID_cudaLaunch_v3020:
                                if (cbInfo->callbackSite == CUPTI_API_ENTER) {
                                    // We cannot allow this thread to take samples since we will be holding a lock which is also needed in the async signal handler
                                    // let no other GPU work get ahead of me
                                    HPCRUN_ASYNC_BLOCK_SPIN_LOCK;
                                    // And disable tracking new threads from CUDA
                                    monitor_disable_new_threads();
                                    // Get CCT node (i.e., kernel launcher)
                                    ucontext_t context;
                                    getcontext(&context);
                                    //cct_node_t *node = hpcrun_gen_thread_ctxt(&context);
                                    cct_node_t *node = hpcrun_sample_callpath(&context, cpu_idle_metric_id, 0, 0 /*skipInner */ , 1 /*isSync */ );
                                    
                                    // Launcher CCT node will be 3 levels above in the loaded module ( Handler -> CUpti -> Cuda -> Launcher )
                                    // TODO: Get correct level .. 3 worked but not on S3D
                                    node = hpcrun_get_cct_node_n_levels_up_in_load_module(node, 0);
                                    
                                    // Get stream Id
                                    err = cuptiGetStreamId(cbInfo->context, (CUstream) TD_GET(active_stream), &streamId);
                                    CHECK_CU_ERROR(err, "cuptiGetStreamId");
                                    // Create a new Cuda Event
                                    event_node = create_and_insert_event(streamId, node);
                                    TD_GET(event_node) = event_node;
                                    fprintf(stderr, "\n Stream = %d", streamId);
                                    // Insert the start event in the stream
                                    err = cudaEventRecord(event_node->event_start, (cudaStream_t) TD_GET(active_stream));
                                    CHECK_CU_ERROR(err, "cudaEventRecord");
                                } else {            // CUPTI_API_EXIT
                                    event_node = (event_list_node *) TD_GET(event_node);
                                    err = cudaEventRecord(event_node->event_end, (cudaStream_t) TD_GET(active_stream));
                                    CHECK_CU_ERROR(err, "cudaEventRecord");
                                    
                                    // enable monitoring new threads
                                    monitor_enable_new_threads();
                                    // let other things be queued into GPU
                                    // safe to take async samples now
                                    HPCRUN_ASYNC_UNBLOCK_SPIN_UNLOCK;
                                }
                                break;
                            default:
                                break;
                        }
                        break;
                        
                    case CUPTI_CB_DOMAIN_RESOURCE:
                        //TMSG(ITIMER_HANDLER, "call back CUPTI_CB_DOMAIN_RESOURCE");
                        //printf("\ncall back CUPTI_CB_DOMAIN_RESOURCE");
                        /*
                         *FIXME: checkout what happens if a stream is being destroyed while another thread
                         is providing work to it.
                         */
                        cbRDInfo = (const CUpti_ResourceData *) cbData;
                        switch (cbid) {
                            case CUPTI_CBID_RESOURCE_STREAM_CREATED:
                                cuptiGetStreamId(cbRDInfo->context, cbRDInfo->resourceHandle.stream, &new_streamId);
                                
                                
                                HPCRUN_ASYNC_BLOCK_SPIN_LOCK;
                                if (g_start_of_world_time == 0) {
                                    // And disable tracking new threads from CUDA
                                    monitor_disable_new_threads();
                                    
                                    // Initialize and Record an event to indicate the start of this stream.
                                    // No need to wait for it since we query only the events posted after this and this will be complete when the latter posted ones are complete.
                                    err = cudaEventCreate(&g_start_of_world_event);
                                    CHECK_CU_ERROR(err, "cudaEventCreate");
                                    
                                    // record time
                                    
                                    struct timeval tv;
                                    gettimeofday(&tv, NULL);
                                    g_start_of_world_time = ((uint64_t)tv.tv_usec 
                                                             + (((uint64_t)tv.tv_sec) * 1000000));
                                    
                                    // record in stream 0	
                                    err = cudaEventRecord(g_start_of_world_event, cbRDInfo->resourceHandle.stream);
                                    CHECK_CU_ERROR(err, "cudaEventRecord");
                                    // enable monitoring new threads
                                    monitor_enable_new_threads();
                                }
                                HPCRUN_ASYNC_UNBLOCK_SPIN_UNLOCK;
                                
                                
                                gpu_trace_open(0, new_streamId);
                                cct_bundle_t *bundle = &(TD_GET(epoch)->csdata);
                                cct_node_t *idl = hpcrun_cct_bundle_get_idle_node(bundle);
                                hpcrun_cct_persistent_id_trace_mutate(idl);
                                gpu_trace_append(0, new_streamId, hpcrun_cct_persistent_id(idl));
                                
                                
                                break;
                            case CUPTI_CBID_RESOURCE_STREAM_DESTROY_STARTING:
                                cuptiGetStreamId(cbRDInfo->context, cbRDInfo->resourceHandle.stream, &new_streamId);
                                //printf("\nStream is getting destroyed: %d\n", new_streamId);
                                ///TODO: evaluate this option : FORCE CLEANUP
                                SYNCHRONOUS_CLEANUP;
                                
                                
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
#if ENABLE_CLEANUP_OPTIMIZATION
                    if (cur_time_us - last_cleanup_time < metric_incr * 0.5) {
                        // reuse the last time recorded num_unfinshed_streams and unfinished_event_list_head
                    } else 
#endif //ENABLE_CLEANUP_OPTIMIZATION
                    {
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
                                .r = metric_incr});
#ifdef NOT_DEFERED
                            // Increment CPU idle cause by metric_incr/num_unfinshed_streams for each of unfinshed_streams
                            for (stream_node * unfinished_stream = unfinished_event_list_head; unfinished_stream; unfinished_stream = unfinished_stream->next_unfinished_stream)
                                cct_metric_data_increment(cpu_idle_cause_metric_id, unfinished_stream->unfinished_event_node->launcher_cct, (cct_metric_data_t) {
                                    .r = metric_incr * 1.0 / num_unfinshed_streams});
#endif
                        } else {
                            // CPU - GPU overlap
                            
                            // Increment cpu_overlap by metric_incr
                            cct_metric_data_increment(cpu_overlap_metric_id, node, (cct_metric_data_t) {
                                .r = metric_incr});
                            // Increment gpu_overlap by metric_incr/num_unfinshed_streams for each of the unfinshed streams
                            for (stream_node * unfinished_stream = unfinished_event_list_head; unfinished_stream; unfinished_stream = unfinished_stream->next_unfinished_stream)
                                cct_metric_data_increment(gpu_overlap_metric_id, unfinished_stream->unfinished_event_node->launcher_cct, (cct_metric_data_t) {
                                    .r = metric_incr * 1.0 / num_unfinshed_streams});
                            
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
