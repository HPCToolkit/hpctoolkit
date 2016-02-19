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
// Copyright ((c)) 2002-2016, Rice University
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
// Blame shifting interface
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
#include <sys/ipc.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>           /* setitimer() */
#include <cuda.h>
#include <cuda_runtime.h>
#include <dlfcn.h>
#include <sys/shm.h>
#include <ucontext.h>           /* struct ucontext */

/******************************************************************************
 * libmonitor
 *****************************************************************************/

#include <monitor.h>

/******************************************************************************
 * local includes
 *****************************************************************************/
#include "common.h"
#include "gpu_blame.h"
#include "gpu_ctxt_actions.h"

#include <hpcrun/main.h>
#include <hpcrun/hpcrun_options.h>
#include <hpcrun/write_data.h>
#include <hpcrun/safe-sampling.h>
#include <hpcrun/hpcrun_stats.h>
#include <hpcrun/memory/mmap.h>

#include <hpcrun/cct/cct.h>
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
#include <lib/prof-lean/wrapper-macros.h>
/******************************************************************************
 * macros
 *****************************************************************************/

// MACROS for error checking CUDA/CUPTI APIs

#define CHECK_CU_ERROR(err, cufunc)                                    \
if (err != CUDA_SUCCESS)                                               \
{                                                                      \
EETMSG("%s:%d: error %d for CUDA Driver API function '%s'\n",          \
__FILE__, __LINE__, err, cufunc);                                      \
monitor_real_abort();                                                  \
}

#define CHECK_CUPTI_ERROR(err, cuptifunc)                             \
if (err != CUPTI_SUCCESS)                                             \
{                                                                     \
const char *errstr;                                                   \
cuptiGetResultString(err, &errstr);                                   \
EEMSG("%s:%d:Error %s for CUPTI API function '%s'.\n",                \
__FILE__, __LINE__, errstr, cuptifunc);                               \
monitor_real_abort();                                                 \
}

#define CU_SAFE_CALL( call ) do {                                  \
CUresult err = call;                                               \
if( CUDA_SUCCESS != err) {                                         \
EEMSG("Cuda driver error %d in call at file '%s' in line %i.\n",   \
err, __FILE__, __LINE__ );                                         \
monitor_real_abort();                                              \
} } while (0)

#define CUDA_SAFE_CALL( call) do {                             \
cudaError_t err = call;                                        \
if( cudaSuccess != err) {                                      \
  EMSG("In %s, @ line %d, gives error %d = '%s'\n", __FILE__, __LINE__, \
                   err, \
                   cudaGetErrorString(err));  \
  monitor_real_abort();                                          \
} } while (0)

#define Cuda_RTcall(fn) cudaRuntimeFunctionPointer[fn ## Enum].fn ## Real

#define GET_STREAM_ID(x) ((x) - g_stream_array)
#define ALL_STREAMS_MASK (0xffffffff)

#define MAX_SHARED_KEY_LENGTH (100)

#define HPCRUN_GPU_SHMSZ (1<<10)

#define SHARED_BLAMING_INITIALISED (ipc_data != NULL)

#define INCR_SHARED_BLAMING_DS(field)  do{ if(SHARED_BLAMING_INITIALISED) atomic_add_i64(&(ipc_data->field), 1L); }while(0) 
#define DECR_SHARED_BLAMING_DS(field)  do{ if(SHARED_BLAMING_INITIALISED) atomic_add_i64(&(ipc_data->field), -1L); }while(0) 

#define ADD_TO_FREE_EVENTS_LIST(node_ptr) do { (node_ptr)->next_free_node = g_free_event_nodes_head; \
g_free_event_nodes_head = (node_ptr); }while(0)

#define ADD_TO_FREE_TREE_NODE_LIST(node_ptr) do { (node_ptr)->next_free_node = g_free_tree_nodes_head; \
g_free_tree_nodes_head = (node_ptr); }while(0)

#define ADD_TO_FREE_ACTIVE_KERNEL_NODE_LIST(node_ptr) do { (node_ptr)->next_free_node = g_free_active_kernel_nodes_head; \
g_free_active_kernel_nodes_head = (node_ptr); }while(0)

#define HPCRUN_ASYNC_BLOCK_SPIN_LOCK bool safe = false; \
do {safe = hpcrun_safe_enter(); \
spinlock_lock(&g_gpu_lock);} while(0)

#define HPCRUN_ASYNC_UNBLOCK_SPIN_UNLOCK  do{spinlock_unlock(&g_gpu_lock); \
    if (safe) hpcrun_safe_exit();} while(0)

#define SYNC_PROLOGUE(ctxt, launch_node, start_time, rec_node)                                                                   \
TD_GET(gpu_data.overload_state) = SYNC_STATE;                                                                                    \
TD_GET(gpu_data.accum_num_sync_threads) = 0;                                                                                     \
TD_GET(gpu_data.accum_num_samples) = 0;                                                                                          \
hpcrun_safe_enter();                                                                                                             \
ucontext_t ctxt;                                                                                                                 \
getcontext(&ctxt);                                                                                                               \
cct_node_t * launch_node = hpcrun_sample_callpath(&ctxt, cpu_idle_metric_id, 0 , 0 /*skipInner */ , 1 /*isSync */ ).sample_node; \
TD_GET(gpu_data.is_thread_at_cuda_sync) = true;                                                                                  \
spinlock_lock(&g_gpu_lock);                                                                                                      \
uint64_t start_time;                                                                                                             \
event_list_node_t  *  rec_node = enter_cuda_sync(& start_time);                                                                  \
spinlock_unlock(&g_gpu_lock);                                                                                                    \
INCR_SHARED_BLAMING_DS(num_threads_at_sync_all_procs);                                                                           \
hpcrun_safe_exit();                                                                                                             

#define SYNC_EPILOGUE(ctxt, launch_node, start_time, rec_node, mask, end_time)                                \
hpcrun_safe_enter();                                                                                          \
spinlock_lock(&g_gpu_lock);                                                                                   \
uint64_t last_kernel_end_time = leave_cuda_sync(rec_node,start_time,mask);                                    \
TD_GET(gpu_data.accum_num_sync_threads) = 0;                                                                  \
TD_GET(gpu_data.accum_num_samples) = 0;                                                                       \
spinlock_unlock(&g_gpu_lock);                                                                                 \
struct timeval tv;                                                                                            \
gettimeofday(&tv, NULL);                                                                                      \
uint64_t end_time  = ((uint64_t)tv.tv_usec + (((uint64_t)tv.tv_sec) * 1000000));                              \
if ( last_kernel_end_time > end_time) {last_kernel_end_time = end_time;}                                      \
uint64_t cpu_idle_time = last_kernel_end_time == 0 ? 0: last_kernel_end_time  - start_time;                   \
uint64_t gpu_idle_time = last_kernel_end_time == 0 ? end_time - start_time : end_time - last_kernel_end_time; \
cct_metric_data_increment(cpu_idle_metric_id, launch_node, (cct_metric_data_t) {.i = (cpu_idle_time)});       \
cct_metric_data_increment(gpu_idle_metric_id, launch_node, (cct_metric_data_t) {.i = (gpu_idle_time)});       \
hpcrun_safe_exit();                                                                                           \
DECR_SHARED_BLAMING_DS(num_threads_at_sync_all_procs);                                                        \
TD_GET(gpu_data.is_thread_at_cuda_sync) = false;

#define SYNC_MEMCPY_PROLOGUE(ctxt, launch_node, start_time, rec_node) SYNC_PROLOGUE(ctxt, launch_node, start_time, rec_node)

#define SYNC_MEMCPY_EPILOGUE(ctxt, launch_node, start_time, rec_node, mask, end_time, bytes, direction)       \
hpcrun_safe_enter();                                                                                          \
spinlock_lock(&g_gpu_lock);                                                                                   \
uint64_t last_kernel_end_time = leave_cuda_sync(rec_node,start_time,mask);                                    \
TD_GET(gpu_data.accum_num_sync_threads) = 0;                                                                  \
TD_GET(gpu_data.accum_num_samples) = 0;                                                                       \
spinlock_unlock(&g_gpu_lock);                                                                                 \
struct timeval tv;                                                                                            \
gettimeofday(&tv, NULL);                                                                                      \
uint64_t end_time  = ((uint64_t)tv.tv_usec + (((uint64_t)tv.tv_sec) * 1000000));                              \
if ( last_kernel_end_time > end_time) {last_kernel_end_time = end_time;}                                      \
uint64_t cpu_idle_time = end_time  - start_time;                                                              \
uint64_t gpu_idle_time = last_kernel_end_time == 0 ? end_time - start_time : end_time - last_kernel_end_time; \
cct_metric_data_increment(cpu_idle_metric_id, launch_node, (cct_metric_data_t) {.i = (cpu_idle_time)});       \
cct_metric_data_increment(gpu_idle_metric_id, launch_node, (cct_metric_data_t) {.i = (gpu_idle_time)});       \
increment_mem_xfer_metric(bytes, direction, launch_node);                                                     \
hpcrun_safe_exit();                                                                                           \
DECR_SHARED_BLAMING_DS(num_threads_at_sync_all_procs);                                                        \
TD_GET(gpu_data.is_thread_at_cuda_sync) = false


#define ASYNC_KERNEL_PROLOGUE(streamId, event_node, context, cct_node, stream, skip_inner)                                              \
create_stream0_if_needed(stream);                                                                                                       \
uint32_t streamId = 0;                                                                                                                  \
event_list_node_t *event_node;                                                                                                          \
streamId = splay_get_stream_id(stream);                                                                         \
HPCRUN_ASYNC_BLOCK_SPIN_LOCK;                                                                                                           \
TD_GET(gpu_data.is_thread_at_cuda_sync) = true;                                                                                         \
ucontext_t context;                                                                                                                     \
getcontext(&context);                                                                                                                   \
cct_node_t *cct_node = hpcrun_sample_callpath(&context, cpu_idle_metric_id, 0, skip_inner /*skipInner */ , 1 /*isSync */ ).sample_node; \
cct_node_t *stream_cct = stream_duplicate_cpu_node(g_stream_array[streamId].st, &context, cct_node);                                    \
monitor_disable_new_threads();                                                                                                          \
event_node = create_and_insert_event(streamId, cct_node, stream_cct);                                                                   \
CUDA_SAFE_CALL(cudaRuntimeFunctionPointer[cudaEventRecordEnum].cudaEventRecordReal(event_node->event_start, stream));                     \
INCR_SHARED_BLAMING_DS(outstanding_kernels)


#define ASYNC_KERNEL_EPILOGUE(event_node, stream)                                                                 \
TD_GET(gpu_data.overload_state) = WORKING_STATE;                                                                  \
CUDA_SAFE_CALL(cudaRuntimeFunctionPointer[cudaEventRecordEnum].cudaEventRecordReal(event_node->event_end, stream)); \
monitor_enable_new_threads();                                                                                     \
TD_GET(gpu_data.is_thread_at_cuda_sync) = false;                                                                  \
HPCRUN_ASYNC_UNBLOCK_SPIN_UNLOCK

#define ASYNC_MEMCPY_PROLOGUE(streamId, event_node, context, cct_node, stream, skip_inner) \
	ASYNC_KERNEL_PROLOGUE(streamId, event_node, context, cct_node, stream, skip_inner)

#define ASYNC_MEMCPY_EPILOGUE(event_node, cct_node, stream, count, kind)                                          \
TD_GET(gpu_data.overload_state) = WORKING_STATE;                                                                  \
CUDA_SAFE_CALL(cudaRuntimeFunctionPointer[cudaEventRecordEnum].cudaEventRecordReal(event_node->event_end, stream)); \
monitor_enable_new_threads();                                                                                     \
increment_mem_xfer_metric(count, kind, cct_node);                                                                 \
TD_GET(gpu_data.is_thread_at_cuda_sync) = false;                                                                  \
HPCRUN_ASYNC_UNBLOCK_SPIN_UNLOCK


#define GET_NEW_TREE_NODE(node_ptr) do {                          \
if (g_free_tree_nodes_head) {                                     \
node_ptr = g_free_tree_nodes_head;                                \
g_free_tree_nodes_head = g_free_tree_nodes_head->next_free_node;  \
} else {                                                          \
node_ptr = (tree_node *) hpcrun_malloc(sizeof(tree_node));        \
}                                                                 \
} while(0)

#define GET_NEW_ACTIVE_KERNEL_NODE(node_ptr) do {                                      \
if (g_free_active_kernel_nodes_head) {                                                 \
node_ptr = g_free_active_kernel_nodes_head;                                            \
g_free_active_kernel_nodes_head = g_free_active_kernel_nodes_head->next_free_node;     \
} else {                                                                               \
node_ptr = (active_kernel_node_t *) hpcrun_malloc(sizeof(active_kernel_node_t));       \
}                                                                                      \
} while(0)


#define SYNCHRONOUS_CLEANUP do{  hpcrun_safe_enter();          \
spinlock_lock(&g_gpu_lock);                                    \
cleanup_finished_events();                                     \
spinlock_unlock(&g_gpu_lock);                                  \
hpcrun_safe_exit(); } while(0)



#define CUDA_RUNTIME_SYNC_WRAPPER(fn,  prologueArgs, epilogueArgs, ...) \
    VA_FN_DECLARE(cudaError_t, fn, __VA_ARGS__) {\
    if (! hpcrun_is_safe_to_sync(__func__)) return VA_FN_CALL(cudaRuntimeFunctionPointer[fn##Enum].fn##Real, __VA_ARGS__);\
    SYNC_PROLOGUE prologueArgs;\
    monitor_disable_new_threads();\
    cudaError_t ret = VA_FN_CALL(cudaRuntimeFunctionPointer[fn##Enum].fn##Real, __VA_ARGS__);\
    monitor_enable_new_threads();\
    SYNC_EPILOGUE epilogueArgs;\
    return ret;\
    }


#define CUDA_RUNTIME_SYNC_ON_STREAM_WRAPPER(fn,  prologueArgs, epilogueArgs, ...) \
    VA_FN_DECLARE(cudaError_t, fn, __VA_ARGS__) {\
    SYNC_PROLOGUE prologueArgs;\
    monitor_disable_new_threads();\
    cudaError_t ret = VA_FN_CALL(cudaRuntimeFunctionPointer[fn##Enum].fn##Real, __VA_ARGS__);\
    hpcrun_safe_enter();\
    uint32_t streamId;\
    streamId = splay_get_stream_id(stream);\
    hpcrun_safe_exit();\
    monitor_enable_new_threads();\
    SYNC_EPILOGUE epilogueArgs;\
    return ret;\
    }


#define CUDA_RUNTIME_ASYNC_MEMCPY_WRAPPER(fn,  prologueArgs, epilogueArgs, ...) \
    VA_FN_DECLARE(cudaError_t, fn, __VA_ARGS__) {\
    if (! hpcrun_is_safe_to_sync(__func__)) return VA_FN_CALL(cudaRuntimeFunctionPointer[fn##Enum].fn##Real, __VA_ARGS__);\
    ASYNC_MEMCPY_PROLOGUE prologueArgs;\
    cudaError_t ret = VA_FN_CALL(cudaRuntimeFunctionPointer[fn##Enum].fn##Real, __VA_ARGS__);\
    ASYNC_MEMCPY_EPILOGUE epilogueArgs;\
    return ret;\
    }

#define CUDA_RUNTIME_SYNC_MEMCPY_WRAPPER(fn,  prologueArgs, epilogueArgs, ...) \
    VA_FN_DECLARE(cudaError_t, fn, __VA_ARGS__) {\
    if (! hpcrun_is_safe_to_sync(__func__)) return VA_FN_CALL(cudaRuntimeFunctionPointer[fn##Enum].fn##Real, __VA_ARGS__);\
    SYNC_MEMCPY_PROLOGUE prologueArgs;\
    monitor_disable_new_threads();\
    cudaError_t ret = VA_FN_CALL(cudaRuntimeFunctionPointer[fn##Enum].fn##Real, __VA_ARGS__);\
    monitor_enable_new_threads();\
    SYNC_MEMCPY_EPILOGUE epilogueArgs;\
    return ret;\
    }


//
// Macro to populate a given set of CUDA function pointers:
//   takes a basename for a function pointer set, and a library
//   to read from (as a fallback position).
//
//  Method:
//
//  Decide on RTLD_NEXT or dlopen of library for the function pointer set
//  (Abort if neither method succeeds)
//  fetch all of the symbols using dlsym, aborting if any failure

#define PopulateGPUFunctionPointers(basename, library)                             \
  char *error;                                                                     \
                                                                                   \
  dlerror(); 									   \
  void* dlsym_arg = RTLD_NEXT;                                                     \
  void* try = dlsym(dlsym_arg, basename ## FunctionPointer[0].functionName);	   \
  if ((error=dlerror()) || (! try)) {						   \
    if (getenv("DEBUG_HPCRUN_GPU_CONS"))					   \
      fprintf(stderr, "RTLD_NEXT argument fails for " #basename " (%s)\n",         \
	      (! try) ? "trial function pointer = NULL" : "dlerror != NULL");	   \
    dlerror();									   \
    dlsym_arg = monitor_real_dlopen(#library, RTLD_LAZY);			   \
    if (! dlsym_arg) {                                                             \
      fprintf(stderr, "fallback dlopen of " #library " failed,"			   \
	      " dlerror message = '%s'\n", dlerror());				   \
      monitor_real_abort();							   \
    }                                                                              \
    if (getenv("DEBUG_HPCRUN_GPU_CONS"))                                           \
      fprintf(stderr, "Going forward with " #basename " overrides using " #library "\n"); \
  }                                                                                \
  else										   \
    if (getenv("DEBUG_HPCRUN_GPU_CONS"))					   \
      fprintf(stderr, "Going forward with " #basename " overrides using RTLD_NEXT\n"); \
  for (int i = 0; i < sizeof(basename ## FunctionPointer)/sizeof(basename ## FunctionPointer[0]); i++) { \
    dlerror();                                                                     \
    basename ## FunctionPointer[i].generic =					   \
      dlsym(dlsym_arg, basename ## FunctionPointer[i].functionName);		   \
    if (getenv("DEBUG_HPCRUN_GPU_CONS"))					   \
      fprintf(stderr, #basename "Fnptr[%d] @ %p for %s = %p\n",                    \
	      i, & basename ## FunctionPointer[i].generic,			   \
	      basename ## FunctionPointer[i].functionName,			   \
	      basename ## FunctionPointer[i].generic);				   \
    if ((error = dlerror()) != NULL) {                                             \
      EEMSG("%s: during dlsym \n", error);					   \
      monitor_real_abort();							   \
    }										   \
  }

/******************************************************************************
 * local constants
 *****************************************************************************/

enum _cuda_const {
    KERNEL_START,
    KERNEL_END
};

// states for accounting overload potential 
enum overloadPotentialState{
    START_STATE=0,
    WORKING_STATE,
    SYNC_STATE,
    OVERLOADABLE_STATE
};


/******************************************************************************
 * externs
 *****************************************************************************/

// function pointers to real cuda runtime functions
extern cudaRuntimeFunctionPointer_t  cudaRuntimeFunctionPointer[];

// function pointers to real cuda driver functions
extern cuDriverFunctionPointer_t  cuDriverFunctionPointer[];

// special papi disable function
extern void hpcrun_disable_papi_cuda(void);

/******************************************************************************
 * forward declarations
 *****************************************************************************/

// Each event_list_node_t maintains information about an asynchronous cuda activity (kernel or memcpy)
// event_list_node_t is a bit of misnomer it should have been activity_list_node_t.

typedef struct event_list_node_t {
    // cudaEvent inserted immediately before and after the activity
    cudaEvent_t event_start;
    cudaEvent_t event_end;

    // start and end times of event_start and event_end
    uint64_t event_start_time;
    uint64_t event_end_time;

    // CCT node of the CPU thread that launched this activity
    cct_node_t *launcher_cct;
    // CCT node of the stream
    cct_node_t *stream_launcher_cct;

    // Outstanding threads that need to examine this activity
    uint32_t ref_count;

    // our internal splay tree id for the corresponding cudaStream for this activity
    uint32_t stream_id;
    union {
        struct event_list_node_t *next;
        struct event_list_node_t *next_free_node;
    };
} event_list_node_t;


// Per GPU stream information
typedef struct stream_node_t {
    // hpcrun profiling and tracing infp
    struct core_profile_trace_data_t *st;
    // pointer to most recently issued activity
    struct event_list_node_t *latest_event_node;
    // pointer to the oldest unfinished activity of this stream
    struct event_list_node_t *unfinished_event_node;
    // pointer to the next stream which has activities pending
    struct stream_node_t *next_unfinished_stream;
    // used to remove from hpcrun cleanup list if stream is explicitly destroyed
    hpcrun_aux_cleanup_t * aux_cleanup_info;
    // IDLE NODE persistent id for this stream
    int32_t idle_node_id;

} stream_node_t;



typedef struct active_kernel_node_t {
    uint64_t event_time;
    bool event_type;
    uint32_t stream_id;
    union {
        cct_node_t *launcher_cct;       // present only in START nodes
        struct active_kernel_node_t *start_node;
    };
    union {
        struct active_kernel_node_t *next;
        struct active_kernel_node_t *next_free_node;
    };
    struct active_kernel_node_t *next_active_kernel;
    struct active_kernel_node_t *prev;
    
} active_kernel_node_t;

// We map GPU stream ID given by cuda to an internal id and place it in a splay tree.
// stream_to_id_map_t is the structure we store as a node in the splay tree

typedef struct stream_to_id_map_t {
    // actual cudaStream
    cudaStream_t stream;
    // Id given by us
    uint32_t id;
    struct stream_to_id_map_t *left;
    struct stream_to_id_map_t *right;
} stream_to_id_map_t;


typedef struct IPC_data_t {
    uint32_t device_id;
    uint64_t outstanding_kernels;
    uint64_t num_threads_at_sync_all_procs;
} IPC_data_t;


static uint32_t cleanup_finished_events();


/******************************************************************************
 * global variables
 *****************************************************************************/


/******************************************************************************
 * local variables
 *****************************************************************************/

// TODO.. Hack to show streams as threads, we  assume max of 32 CPU threads
static uint32_t g_stream_id = 32;
static uint32_t g_stream_to_id_index = 0;

// Lock for stream to id map
static spinlock_t g_stream_id_lock = SPINLOCK_UNLOCKED;

// lock for GPU activities
static spinlock_t g_gpu_lock = SPINLOCK_UNLOCKED;

static uint64_t g_num_threads_at_sync;

static event_list_node_t *g_free_event_nodes_head;
static active_kernel_node_t *g_free_active_kernel_nodes_head;

// root of splay tree of stream ids
static struct stream_to_id_map_t *stream_to_id_tree_root;
static stream_to_id_map_t stream_to_id[MAX_STREAMS];
static stream_node_t g_stream_array[MAX_STREAMS];

// First stream with pending activities
static stream_node_t *g_unfinished_stream_list_head;

// Last stream with pending activities
static event_list_node_t *g_finished_event_nodes_tail;

// dummy activity node
static event_list_node_t dummy_event_node = {
  .event_end = 0,
  .event_start = 0,
  .event_end_time = 0,
  .event_end_time = 0,
  .launcher_cct = 0,
  .stream_launcher_cct = 0 
};

// is inter-process blaming enabled?
static bool g_do_shared_blaming;

// What level of nodes to skip in the backtrace
static uint32_t g_cuda_launch_skip_inner;

static uint64_t g_start_of_world_time;

static cudaEvent_t g_start_of_world_event;

static bool g_stream0_initialized = false;

static IPC_data_t * ipc_data;

/******************** Utilities ********************/
/******************** CONSTRUCTORS ********************/


// obtain function pointers to all real cuda runtime functions

static void
PopulateEntryPointesToWrappedCudaRuntimeCalls()
{
  PopulateGPUFunctionPointers(cudaRuntime, libcudart.so)
}

// obtain function pointers to all real cuda driver functions

static void
PopulateEntryPointesToWrappedCuDriverCalls(void)
{
  PopulateGPUFunctionPointers(cuDriver, libcuda.so)
}

static void
InitCpuGpuBlameShiftDataStructs(void)
{
    char * shared_blaming_env;
    char * cuda_launch_skip_inner_env;
    g_unfinished_stream_list_head = NULL;
    g_finished_event_nodes_tail = &dummy_event_node;
    dummy_event_node.next = g_finished_event_nodes_tail;
    shared_blaming_env = getenv("HPCRUN_ENABLE_SHARED_GPU_BLAMING");
    if(shared_blaming_env)
        g_do_shared_blaming = atoi(shared_blaming_env);

    cuda_launch_skip_inner_env = getenv("HPCRUN_CUDA_LAUNCH_SKIP_INNER");
    if(cuda_launch_skip_inner_env)
        g_cuda_launch_skip_inner = atoi(cuda_launch_skip_inner_env);
}


static void PopulateEntryPointesToWrappedCalls() {
    PopulateEntryPointesToWrappedCudaRuntimeCalls();
    PopulateEntryPointesToWrappedCuDriverCalls();
}

__attribute__((constructor))
static void
CpuGpuBlameShiftInit(void)
{
  hpcrun_disable_papi_cuda();
  if (getenv("DEBUG_HPCRUN_GPU_CONS"))
    fprintf(stderr, "CPU-GPU blame shift constructor called\n");
  // no dlopen calls in static case
  // #ifndef HPCRUN_STATIC_LINK
  PopulateEntryPointesToWrappedCalls();
  InitCpuGpuBlameShiftDataStructs();
  // #endif // ! HPCRUN_STATIC_LINK
}

/******************** END CONSTRUCTORS ****/

static char shared_key[MAX_SHARED_KEY_LENGTH];

static void destroy_shared_memory(void * p) {
    // we should munmap, but I will not do since we dont do it in so many other places in hpcrun
    // munmap(ipc_data);
    shm_unlink((char *)shared_key);
}

static inline void create_shared_memory() {
  
    int device_id;
    int fd ;
    monitor_disable_new_threads();
    CUDA_SAFE_CALL(cudaRuntimeFunctionPointer[cudaGetDeviceEnum].cudaGetDeviceReal(&device_id));
    monitor_enable_new_threads();
    sprintf(shared_key, "/gpublame%d",device_id);
    if ( (fd = shm_open(shared_key, O_RDWR | O_CREAT, 0666)) < 0 ) {
        EEMSG("Failed to shm_open (%s) on device %d, retval = %d", shared_key, device_id, fd);
        monitor_real_abort();
    }
    if ( ftruncate(fd, sizeof(IPC_data_t)) < 0 ) {
        EEMSG("Failed to ftruncate() on device %d",device_id);
        monitor_real_abort();
    }
    
    if( (ipc_data = mmap(NULL, sizeof(IPC_data_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 )) == MAP_FAILED ) {
        EEMSG("Failed to mmap() on device %d",device_id);
        monitor_real_abort();
    }

    hpcrun_process_aux_cleanup_add(destroy_shared_memory, (void *) shared_key);
    
}

// Get the stream id given a cudaStream

static struct stream_to_id_map_t *splay(struct stream_to_id_map_t *root, cudaStream_t key) {
    REGULAR_SPLAY_TREE(stream_to_id_map_t, root, key, stream, left, right);
    return root;
}


static uint32_t splay_get_stream_id(cudaStream_t key) {
    spinlock_lock(&g_stream_id_lock);
    struct stream_to_id_map_t *root = stream_to_id_tree_root;
    REGULAR_SPLAY_TREE(stream_to_id_map_t, root, key, stream, left, right);
    // The stream at the root must match the key, else we are in a bad shape.
    assert(root->stream == key);
    stream_to_id_tree_root = root;
    uint32_t ret = stream_to_id_tree_root->id;
    spinlock_unlock(&g_stream_id_lock);
    return ret;
    
}


// Insert a new cudaStream into the splay tree

static stream_to_id_map_t *splay_insert(cudaStream_t stream_ip)
{
    
    spinlock_lock(&g_stream_id_lock);
    struct stream_to_id_map_t *node = &stream_to_id[g_stream_to_id_index++];
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
            EEMSG("stream_to_id_tree_root  splay tree: unable to insert %p (already present)", node->stream);
            monitor_real_abort();
        }
    }
    stream_to_id_tree_root = node;
    spinlock_unlock(&g_stream_id_lock);
    return stream_to_id_tree_root;
}

// Initialize hpcrun core_profile_trace_data for a new stream
static inline core_profile_trace_data_t *hpcrun_stream_data_alloc_init(int id) {
    core_profile_trace_data_t *st = hpcrun_mmap_anon(sizeof(core_profile_trace_data_t));
    // FIXME: revisit to perform this memstore operation appropriately.
    //memstore = td->memstore;
    memset(st, 0xfe, sizeof(core_profile_trace_data_t));
    //td->memstore = memstore;
    //hpcrun_make_memstore(&td->memstore, is_child);
    st->id = id;
    st->epoch = hpcrun_malloc(sizeof(epoch_t));
    st->epoch->csdata_ctxt = copy_thr_ctxt(TD_GET(core_profile_trace_data.epoch)->csdata.ctxt); //copy_thr_ctxt(thr_ctxt);
    hpcrun_cct_bundle_init(&(st->epoch->csdata), (st->epoch->csdata).ctxt);
    st->epoch->loadmap = hpcrun_getLoadmap();
    st->epoch->next  = NULL;
    hpcrun_cct2metrics_init(&(st->cct2metrics_map)); //this just does st->map = NULL;
    
    
    st->trace_min_time_us = 0;
    st->trace_max_time_us = 0;
    st->hpcrun_file  = NULL;
    
    return st;
}



static cct_node_t *stream_duplicate_cpu_node(core_profile_trace_data_t *st, ucontext_t *context, cct_node_t *node) {
    cct_bundle_t* cct= &(st->epoch->csdata);
    cct_node_t * tmp_root = cct->tree_root;
    hpcrun_cct_insert_path(&tmp_root, node);
    return tmp_root;
}


inline void hpcrun_stream_finalize(void * st) {
    if(hpcrun_trace_isactive()) 
      hpcrun_trace_close(st);
 
    hpcrun_write_profile_data((core_profile_trace_data_t *) st);
}


static struct stream_to_id_map_t *splay_delete(cudaStream_t stream)
{
    struct stream_to_id_map_t *result = NULL;
    
    TMSG(CUDA, "Trying to delete %p from stream splay tree", stream);
    spinlock_lock(&g_stream_id_lock);
    if (stream_to_id_tree_root == NULL) {
        spinlock_unlock(&g_stream_id_lock);
        TMSG(CUDA, "stream_to_id_map_t splay tree empty: unable to delete %p", stream);
        return NULL;
    }
    
    stream_to_id_tree_root = splay(stream_to_id_tree_root, stream);
    
    if (stream != stream_to_id_tree_root->stream) {
        spinlock_unlock(&g_stream_id_lock);
        TMSG(CUDA, "trying to deleting stream %p, but not in splay tree (root = %p)", stream, stream_to_id_tree_root->stream);
	//        monitor_real_abort();
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


// Prologue for any cuda synchronization routine
static inline event_list_node_t *enter_cuda_sync(uint64_t * syncStart) {
    /// caller does HPCRUN_ASYNC_BLOCK_SPIN_LOCK;
    
    // Cleanup events so that when I goto wait anybody in the queue will be the ones I have not seen and finished after my timer started.
    cleanup_finished_events();
    
    struct timeval tv;
    gettimeofday(&tv, NULL);
    *syncStart = ((uint64_t) tv.tv_usec + (((uint64_t) tv.tv_sec) * 1000000));
    
    event_list_node_t *recorded_node = g_finished_event_nodes_tail;
    if (g_finished_event_nodes_tail != &dummy_event_node)
        g_finished_event_nodes_tail->ref_count++;
    
    atomic_add_i64(&g_num_threads_at_sync, 1L);
    // caller does  HPCRUN_ASYNC_UNBLOCK_SPIN_UNLOCK;
    return recorded_node;
}


// blame all kernels finished during the sync time

static uint64_t attribute_shared_blame_on_kernels(event_list_node_t * recorded_node, uint64_t recorded_time, const uint32_t stream_mask, double scaling_factor) {
    
    // if recorded_node is not dummy_event_node decrement its ref count
    if (recorded_node != &dummy_event_node)
        recorded_node->ref_count--;
    
    uint32_t num_active_kernels = 0;
    active_kernel_node_t *sorted_active_kernels_begin = NULL;
    
    // Traverse all nodes, inserting them in a sorted list if their end times were past the recorded time
    // If their start times were before the recorded, just record them as recorded_time
    
    event_list_node_t *cur = recorded_node->next, *prev = recorded_node;
    while (cur != &dummy_event_node) {
        // if the node's refcount is already zero, then free it and we dont care about it
        if (cur->ref_count == 0) {
            prev->next = cur->next;
            event_list_node_t *to_free = cur;
            cur = cur->next;
            ADD_TO_FREE_EVENTS_LIST(to_free);
            continue;
        }
        
        cur->ref_count--;
        
        if ((cur->event_end_time <= recorded_time) || (cur->stream_id != (cur->stream_id & stream_mask))) {
            if (cur->ref_count == 0) {
                prev->next = cur->next;
                event_list_node_t *to_free = cur;
                cur = cur->next;
                ADD_TO_FREE_EVENTS_LIST(to_free);
            } else {
                prev = cur;
                cur = cur->next;
            }
            continue;
        }
        // Add start and end times in a sorted list (insertion sort)
        
        active_kernel_node_t *start_active_kernel_node;
        active_kernel_node_t *end_active_kernel_node;
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
                event_list_node_t *to_free = cur;
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
            active_kernel_node_t *current = sorted_active_kernels_begin->prev;
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
            event_list_node_t *to_free = cur;
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
        active_kernel_node_t *dummy_kernel_node;
        GET_NEW_ACTIVE_KERNEL_NODE(dummy_kernel_node);
        sorted_active_kernels_begin->prev->next = dummy_kernel_node;
        dummy_kernel_node->prev = sorted_active_kernels_begin->prev;
        sorted_active_kernels_begin->prev = dummy_kernel_node;
        dummy_kernel_node->next = sorted_active_kernels_begin;
        
        active_kernel_node_t *current = sorted_active_kernels_begin;
        uint64_t last_time = recorded_time;
        do {
            uint64_t new_time = current->event_time;
            
            assert(new_time >= last_time);
            assert(current != dummy_kernel_node && "should never process dummy_kernel_node");
            
            if (num_active_kernels && (new_time > last_time)) {
                //blame all
                active_kernel_node_t *blame_node = current->prev;
                do {
                    assert(blame_node->event_type == KERNEL_START);
                    
                    cct_metric_data_increment(cpu_idle_cause_metric_id, blame_node->launcher_cct, (cct_metric_data_t) {
                        .r = (new_time - last_time) * (scaling_factor) / num_active_kernels}
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
                active_kernel_node_t *to_free = current;
                current = current->next;
                ADD_TO_FREE_ACTIVE_KERNEL_NODE_LIST(to_free);
                
            }
            
        } while (current != sorted_active_kernels_begin->prev);
        // Free up the dummy node
        ADD_TO_FREE_ACTIVE_KERNEL_NODE_LIST(dummy_kernel_node);
        
    }
    
    return last_kernel_end_time;
    
}

// Epilogue for any cuda synchronization routine
static inline uint64_t leave_cuda_sync(event_list_node_t * recorded_node, uint64_t syncStart, const uint32_t stream_mask) {
    //caller does       HPCRUN_ASYNC_BLOCK_SPIN_LOCK;
    
    // Cleanup events so that when I goto wait anybody in the queue will be the ones I have not seen and finished after my timer started.
    cleanup_finished_events();
     
    double scaling_factor = 1.0; 
    if(SHARED_BLAMING_INITIALISED && TD_GET(gpu_data.accum_num_samples))
      scaling_factor *= ((double)TD_GET(gpu_data.accum_num_sync_threads))/TD_GET(gpu_data.accum_num_samples);  
    uint64_t last_kernel_end_time = attribute_shared_blame_on_kernels(recorded_node, syncStart, stream_mask, scaling_factor);
    atomic_add_i64(&g_num_threads_at_sync, -1L);
    return last_kernel_end_time;
    //caller does       HPCRUN_ASYNC_UNBLOCK_SPIN_UNLOCK;
}


// inspect activities finished on each stream and record metrics accordingly

static uint32_t cleanup_finished_events() {
    uint32_t num_unfinished_streams = 0;
    stream_node_t *prev_stream = NULL;
    stream_node_t *next_stream = NULL;
    stream_node_t *cur_stream = g_unfinished_stream_list_head;

    while (cur_stream != NULL) {
        assert(cur_stream->unfinished_event_node && " Can't point unfinished stream to null");
        next_stream = cur_stream->next_unfinished_stream;
        
        event_list_node_t *current_event = cur_stream->unfinished_event_node;
        while (current_event) {
            
            cudaError_t err_cuda = cudaRuntimeFunctionPointer[cudaEventQueryEnum].cudaEventQueryReal(current_event->event_end);
            
            if (err_cuda == cudaSuccess) {
                
                // Decrement   ipc_data->outstanding_kernels
                DECR_SHARED_BLAMING_DS(outstanding_kernels);
                
                // record start time
                float elapsedTime;      // in millisec with 0.5 microsec resolution as per CUDA
                
                //FIX ME: deleting Elapsed time to handle context destruction....
                //static uint64_t deleteMeTime = 0;
		TMSG(CUDA, "BEFORE: EventElapsedRT(%p, %p)\n", g_start_of_world_event, current_event->event_start);
		cudaError_t err1 = Cuda_RTcall(cudaEventElapsedTime)(&elapsedTime,
								   g_start_of_world_event,
								   current_event->event_start);
		// soft failure
		if (err1 != cudaSuccess) {
		  EMSG("cudaEventElaspsedTime failed");
		  break;
		}
                
                assert(elapsedTime > 0);
                
                uint64_t micro_time_start = (uint64_t) (((double) elapsedTime) * 1000) + g_start_of_world_time;
                
                CUDA_SAFE_CALL(cudaRuntimeFunctionPointer[cudaEventElapsedTimeEnum].cudaEventElapsedTimeReal(&elapsedTime, g_start_of_world_event, current_event->event_end));
                
                assert(elapsedTime > 0);
                uint64_t micro_time_end = (uint64_t) (((double) elapsedTime) * 1000) + g_start_of_world_time;
                
                assert(micro_time_start <= micro_time_end);
                
                if(hpcrun_trace_isactive()) {
                  hpcrun_trace_append_with_time(cur_stream->st, cur_stream->idle_node_id, HPCRUN_FMT_MetricId_NULL /* null metric id */, micro_time_start - 1);
                    
                  cct_node_t *stream_cct = current_event->stream_launcher_cct;
                    
                  hpcrun_cct_persistent_id_trace_mutate(stream_cct);
                    
                  hpcrun_trace_append_with_time(cur_stream->st, hpcrun_cct_persistent_id(stream_cct), HPCRUN_FMT_MetricId_NULL /* null metric id */, micro_time_start);
                    
                  hpcrun_trace_append_with_time(cur_stream->st, hpcrun_cct_persistent_id(stream_cct), HPCRUN_FMT_MetricId_NULL /* null metric id */, micro_time_end);
                    
                  hpcrun_trace_append_with_time(cur_stream->st, cur_stream->idle_node_id, HPCRUN_FMT_MetricId_NULL /* null metric id */, micro_time_end + 1);
                }
                
                
                // Add the kernel execution time to the gpu_time_metric_id
                cct_metric_data_increment(gpu_time_metric_id, current_event->launcher_cct, (cct_metric_data_t) {
                    .i = (micro_time_end - micro_time_start)});
                
                event_list_node_t *deferred_node = current_event;
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
                    // It is better not to call destroy from here since we might be in the signal handler
                    // Events will be destroyed lazily when they need to be reused.
                    ADD_TO_FREE_EVENTS_LIST(deferred_node);
                }
                
            } else {
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


// Insert a new activity in a stream
// Caller is responsible for calling monitor_disable_new_threads()
static event_list_node_t *create_and_insert_event(int stream_id, cct_node_t * launcher_cct, cct_node_t * stream_launcher_cct) {
    
    event_list_node_t *event_node;
    if (g_free_event_nodes_head) {
        // get from free list
        event_node = g_free_event_nodes_head;
        g_free_event_nodes_head = g_free_event_nodes_head->next_free_node;

        // Free the old events if they are alive
        if (event_node->event_start)
          CUDA_SAFE_CALL(cudaRuntimeFunctionPointer[cudaEventDestroyEnum].cudaEventDestroyReal((event_node->event_start)));
        if (event_node->event_end)
          CUDA_SAFE_CALL(cudaRuntimeFunctionPointer[cudaEventDestroyEnum].cudaEventDestroyReal((event_node->event_end)));
        
    } else {
        // allocate new node
        event_node = (event_list_node_t *) hpcrun_malloc(sizeof(event_list_node_t));
    }
    //cudaError_t err =  cudaEventCreateWithFlags(&(event_node->event_end),cudaEventDisableTiming);
    
    CUDA_SAFE_CALL(cudaRuntimeFunctionPointer[cudaEventCreateEnum].cudaEventCreateReal(&(event_node->event_start)));
    CUDA_SAFE_CALL(cudaRuntimeFunctionPointer[cudaEventCreateEnum].cudaEventCreateReal(&(event_node->event_end)));
    
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

static void close_all_streams(stream_to_id_map_t *root) {

    if (!root)
        return;

    close_all_streams(root->left);
    close_all_streams(root->right);
    uint32_t streamId;
    streamId = root->id;

    hpcrun_stream_finalize(g_stream_array[streamId].st);

    // remove from hpcrun process auxiliary cleanup list 
    hpcrun_process_aux_cleanup_remove(g_stream_array[streamId].aux_cleanup_info);

    g_stream_array[streamId].st = NULL;
}


// Stream #0 is never explicitly created. Hence create it if needed.
// An alternate option is to create it eagerly whether needed or not.

static void create_stream0_if_needed(cudaStream_t stream) {
    HPCRUN_ASYNC_BLOCK_SPIN_LOCK;
    if ( (((uint64_t)stream) == 0 )&& (g_stream0_initialized == false)) {
        uint32_t new_streamId;
        new_streamId = splay_insert(0)->id;
        if (g_start_of_world_time == 0) {
            
            
            
            // And disable tracking new threads from CUDA
            monitor_disable_new_threads();
            
            // Initialize and Record an event to indicate the start of this stream.
            // No need to wait for it since we query only the events posted after this and this will be complete when the latter posted ones are complete.
            
            CUDA_SAFE_CALL(cudaRuntimeFunctionPointer[cudaEventCreateEnum].cudaEventCreateReal(&g_start_of_world_event));
            
            // record time
            
            struct timeval tv;
            gettimeofday(&tv, NULL);
            g_start_of_world_time = ((uint64_t) tv.tv_usec + (((uint64_t) tv.tv_sec) * 1000000));
            
            // record in stream 0
            CUDA_SAFE_CALL(cudaRuntimeFunctionPointer[cudaEventRecordEnum].cudaEventRecordReal(g_start_of_world_event, 0));
            
            // enable monitoring new threads
            monitor_enable_new_threads();
            
            // This is a good time to create the shared memory
            // FIX ME: DEVICE_ID should be derived
            if(g_do_shared_blaming && ipc_data == NULL)
                create_shared_memory();
            
            
        }
        
        struct timeval tv;
        gettimeofday(&tv, NULL);
        g_stream_array[new_streamId].st = hpcrun_stream_data_alloc_init(new_streamId);
        
        if(hpcrun_trace_isactive()) {
            hpcrun_trace_open(g_stream_array[new_streamId].st);
            
            /*FIXME: convert below 4 lines to a macro */
            cct_bundle_t *bundle = &(g_stream_array[new_streamId].st->epoch->csdata);
            cct_node_t *idl = hpcrun_cct_bundle_get_idle_node(bundle);
            hpcrun_cct_persistent_id_trace_mutate(idl);
            // store the persistent id one time
            g_stream_array[new_streamId].idle_node_id = hpcrun_cct_persistent_id(idl);
            
            hpcrun_trace_append(g_stream_array[new_streamId].st, g_stream_array[new_streamId].idle_node_id, HPCRUN_FMT_MetricId_NULL /* null metric id */);
            
        }

        g_stream_array[new_streamId].aux_cleanup_info = hpcrun_process_aux_cleanup_add(hpcrun_stream_finalize, g_stream_array[new_streamId].st);
        g_stream0_initialized = true;
    }
    HPCRUN_ASYNC_UNBLOCK_SPIN_UNLOCK;
    
}


////////////////////////////////////////////////
// CUDA Runtime overrides
////////////////////////////////////////////////


CUDA_RUNTIME_SYNC_WRAPPER(cudaThreadSynchronize, (context, launcher_cct,
syncStart, recorded_node), (context, launcher_cct, syncStart,
recorded_node, ALL_STREAMS_MASK, syncEnd), void)

CUDA_RUNTIME_SYNC_ON_STREAM_WRAPPER(cudaStreamSynchronize, (context,
launcher_cct, syncStart, recorded_node), (context, launcher_cct,
syncStart, recorded_node, streamId, syncEnd), cudaStream_t, stream)

CUDA_RUNTIME_SYNC_WRAPPER(cudaEventSynchronize, (context, launcher_cct,
syncStart, recorded_node), (context, launcher_cct, syncStart,
recorded_node, ALL_STREAMS_MASK, syncEnd), cudaEvent_t, event)

CUDA_RUNTIME_SYNC_ON_STREAM_WRAPPER(cudaStreamWaitEvent, (context,
launcher_cct, syncStart, recorded_node), (context, launcher_cct,
syncStart, recorded_node, streamId, syncEnd), cudaStream_t, stream,
cudaEvent_t, event, unsigned int, flags)

CUDA_RUNTIME_SYNC_WRAPPER(cudaDeviceSynchronize, (context, launcher_cct,
syncStart, recorded_node), (context, launcher_cct, syncStart,
recorded_node, ALL_STREAMS_MASK, syncEnd), void)

CUDA_RUNTIME_SYNC_WRAPPER(cudaMallocArray, (context, launcher_cct,
syncStart, recorded_node), (context, launcher_cct, syncStart,
recorded_node, ALL_STREAMS_MASK, syncEnd), struct cudaArray **, array,
const struct cudaChannelFormatDesc *, desc, size_t, width, size_t,
height, unsigned int, flags)

CUDA_RUNTIME_SYNC_WRAPPER(cudaFree, (context, launcher_cct, syncStart,
recorded_node), (context, launcher_cct, syncStart, recorded_node,
ALL_STREAMS_MASK, syncEnd), void *, devPtr)

CUDA_RUNTIME_SYNC_WRAPPER(cudaFreeArray, (context, launcher_cct,
syncStart, recorded_node), (context, launcher_cct, syncStart,
recorded_node, ALL_STREAMS_MASK, syncEnd),struct cudaArray *, array)



cudaError_t cudaConfigureCall(dim3 grid, dim3 block, size_t mem, cudaStream_t stream) {
    
    if (! hpcrun_is_safe_to_sync(__func__))
      return cudaRuntimeFunctionPointer[cudaConfigureCallEnum].cudaConfigureCallReal(grid, block, mem, stream);
    TD_GET(gpu_data.is_thread_at_cuda_sync) = true;
    monitor_disable_new_threads();
    cudaError_t ret = cudaRuntimeFunctionPointer[cudaConfigureCallEnum].cudaConfigureCallReal(grid, block, mem, stream);
    monitor_enable_new_threads();
    TD_GET(gpu_data.is_thread_at_cuda_sync) = false;
    TD_GET(gpu_data.active_stream) = (uint64_t) stream;
    return ret;
}

#if (CUDART_VERSION < 5000)
    cudaError_t cudaLaunch(const char *entry) {
#else
    cudaError_t cudaLaunch(const void *entry) {
#endif
    
    if (! hpcrun_is_safe_to_sync(__func__))
      return cudaRuntimeFunctionPointer[cudaLaunchEnum].cudaLaunchReal(entry);
    TMSG(CPU_GPU,"Cuda launch (get spinlock)");
    ASYNC_KERNEL_PROLOGUE(streamId, event_node, context, cct_node, ((cudaStream_t) (TD_GET(gpu_data.active_stream))), g_cuda_launch_skip_inner);
    
    cudaError_t ret = cudaRuntimeFunctionPointer[cudaLaunchEnum].cudaLaunchReal(entry);
    
    TMSG(CPU_GPU, "Cuda launch about to release spin lock");
    ASYNC_KERNEL_EPILOGUE(event_node, ((cudaStream_t) (TD_GET(gpu_data.active_stream))));
    TMSG(CPU_GPU, "Cuda launch done !(spin lock released)");

    return ret;
}


cudaError_t cudaStreamDestroy(cudaStream_t stream) {
    
    SYNCHRONOUS_CLEANUP;
    
    hpcrun_safe_enter();
    
    uint32_t streamId;
    
    streamId = splay_get_stream_id(stream);
    
    hpcrun_stream_finalize(g_stream_array[streamId].st);

    // remove from hpcrun process auxiliary cleanup list 
    hpcrun_process_aux_cleanup_remove(g_stream_array[streamId].aux_cleanup_info);

    g_stream_array[streamId].st = NULL;
    
    monitor_disable_new_threads();
    cudaError_t ret = cudaRuntimeFunctionPointer[cudaStreamDestroyEnum].cudaStreamDestroyReal(stream);
    monitor_enable_new_threads();
    
    // Delete splay tree entry
    splay_delete(stream);
    hpcrun_safe_exit();
    return ret;
    
}


static void StreamCreateBookKeeper(cudaStream_t * stream){
    uint32_t new_streamId = splay_insert(*stream)->id;
    HPCRUN_ASYNC_BLOCK_SPIN_LOCK;
    if (g_start_of_world_time == 0) {
        // In case cudaLaunch causes dlopn, async block may get enabled, as a safety net set gpu_data.is_thread_at_cuda_sync so that we dont call any cuda calls
        TD_GET(gpu_data.is_thread_at_cuda_sync) = true;
        
        // And disable tracking new threads from CUDA
        monitor_disable_new_threads();
        
        // Initialize and Record an event to indicate the start of this stream.
        // No need to wait for it since we query only the events posted after this and this will be complete when the latter posted ones are complete.
        CUDA_SAFE_CALL(cudaRuntimeFunctionPointer[cudaEventCreateEnum].cudaEventCreateReal(&g_start_of_world_event));
        
        // record time
        
        struct timeval tv;
        gettimeofday(&tv, NULL);
        g_start_of_world_time = ((uint64_t) tv.tv_usec + (((uint64_t) tv.tv_sec) * 1000000));
        
        // record in stream 0
        CUDA_SAFE_CALL(cudaRuntimeFunctionPointer[cudaEventRecordEnum].cudaEventRecordReal(g_start_of_world_event, 0));

        // enable monitoring new threads
        monitor_enable_new_threads();
        
        // This is a good time to create the shared memory
        // FIX ME: DEVICE_ID should be derived
        if(g_do_shared_blaming && ipc_data == NULL)
            create_shared_memory();
        
        // Ok to call cuda functions from the signal handler
        TD_GET(gpu_data.is_thread_at_cuda_sync) = false;
        
    }
    
    g_stream_array[new_streamId].st = hpcrun_stream_data_alloc_init(new_streamId);
    if(hpcrun_trace_isactive()) {
        hpcrun_trace_open(g_stream_array[new_streamId].st);
        
        /*FIXME: convert below 4 lines to a macro */
        cct_bundle_t *bundle = &(g_stream_array[new_streamId].st->epoch->csdata);
        cct_node_t *idl = hpcrun_cct_bundle_get_idle_node(bundle);
        hpcrun_cct_persistent_id_trace_mutate(idl);
        // store the persistent id one time.
        g_stream_array[new_streamId].idle_node_id = hpcrun_cct_persistent_id(idl);
        hpcrun_trace_append(g_stream_array[new_streamId].st, g_stream_array[new_streamId].idle_node_id, HPCRUN_FMT_MetricId_NULL /* null metric id */);
        
    }
    
    g_stream_array[new_streamId].aux_cleanup_info = hpcrun_process_aux_cleanup_add(hpcrun_stream_finalize, g_stream_array[new_streamId].st);
    HPCRUN_ASYNC_UNBLOCK_SPIN_UNLOCK;

}

cudaError_t cudaStreamCreate(cudaStream_t * stream) {
    
    TD_GET(gpu_data.is_thread_at_cuda_sync) = true;
    monitor_disable_new_threads();
    cudaError_t ret = cudaRuntimeFunctionPointer[cudaStreamCreateEnum].cudaStreamCreateReal(stream);
    monitor_enable_new_threads();
    TD_GET(gpu_data.is_thread_at_cuda_sync) = false;
    
    StreamCreateBookKeeper(stream);
    return ret;
}

inline static void increment_mem_xfer_metric(size_t count, enum cudaMemcpyKind kind, cct_node_t *node){
    switch(kind){
        case cudaMemcpyHostToHost:
            cct_metric_data_increment(h_to_h_data_xfer_metric_id, node, (cct_metric_data_t) {.i = (count)});
            break;

        case cudaMemcpyHostToDevice:
            cct_metric_data_increment(h_to_d_data_xfer_metric_id, node, (cct_metric_data_t) {.i = (count)});
            break;


        case cudaMemcpyDeviceToHost:
            cct_metric_data_increment(d_to_h_data_xfer_metric_id, node, (cct_metric_data_t) {.i = (count)});
            break;

        case cudaMemcpyDeviceToDevice:
            cct_metric_data_increment(d_to_d_data_xfer_metric_id, node, (cct_metric_data_t) {.i = (count)});
            break;

        case cudaMemcpyDefault:
            cct_metric_data_increment(uva_data_xfer_metric_id, node, (cct_metric_data_t) {.i = (count)});
            break;

        default : break;
            
    }
}



CUDA_RUNTIME_SYNC_WRAPPER(cudaMalloc, (context, launcher_cct, syncStart,
recorded_node), (context, launcher_cct, syncStart, recorded_node,
ALL_STREAMS_MASK, syncEnd), void **, devPtr, size_t, size)

CUDA_RUNTIME_SYNC_WRAPPER(cudaMalloc3D, (context, launcher_cct, syncStart,
recorded_node), (context, launcher_cct, syncStart, recorded_node,
ALL_STREAMS_MASK, syncEnd), struct cudaPitchedPtr*, pitchedDevPtr,
struct cudaExtent, extent)

CUDA_RUNTIME_SYNC_WRAPPER(cudaMalloc3DArray, (context, launcher_cct,
syncStart, recorded_node), (context, launcher_cct, syncStart,
recorded_node, ALL_STREAMS_MASK, syncEnd), struct cudaArray**, array,
const struct cudaChannelFormatDesc*, desc, struct cudaExtent, extent,
unsigned int, flags)

CUDA_RUNTIME_SYNC_MEMCPY_WRAPPER(cudaMemcpy3D, (context, launcher_cct,
syncStart, recorded_node), (context, launcher_cct, syncStart,
recorded_node, ALL_STREAMS_MASK, syncEnd, (p->extent.width *
p->extent.height * p->extent.depth), (p->kind)), const struct
cudaMemcpy3DParms *, p)


CUDA_RUNTIME_SYNC_MEMCPY_WRAPPER(cudaMemcpy3DPeer, (context,
launcher_cct, syncStart, recorded_node), (context, launcher_cct,
syncStart, recorded_node, ALL_STREAMS_MASK, syncEnd, (p->extent.width *
p->extent.height * p->extent.depth), cudaMemcpyDeviceToDevice), const
struct cudaMemcpy3DPeerParms *, p)

CUDA_RUNTIME_ASYNC_MEMCPY_WRAPPER(cudaMemcpy3DAsync, (streamId,
event_node, context, cct_node, stream, 0), (event_node, cct_node, stream,
(p->extent.width * p->extent.height * p->extent.depth), (p->kind)),
const struct cudaMemcpy3DParms *, p, cudaStream_t,  stream)

CUDA_RUNTIME_ASYNC_MEMCPY_WRAPPER(cudaMemcpy3DPeerAsync, (streamId,
event_node, context, cct_node, stream, 0), (event_node, cct_node,
stream, (p->extent.width * p->extent.height * p->extent.depth),
cudaMemcpyDeviceToDevice), const struct cudaMemcpy3DPeerParms *, p,
cudaStream_t, stream)

CUDA_RUNTIME_SYNC_MEMCPY_WRAPPER(cudaMemcpyPeer, (context,
launcher_cct, syncStart, recorded_node), (context, launcher_cct,
syncStart, recorded_node, ALL_STREAMS_MASK, syncEnd, count,
cudaMemcpyDeviceToDevice), void *, dst, int, dstDevice, const void *,
src, int, srcDevice, size_t, count)

CUDA_RUNTIME_SYNC_MEMCPY_WRAPPER(cudaMemcpyFromArray, (context,
launcher_cct, syncStart, recorded_node), (context, launcher_cct,
syncStart, recorded_node, ALL_STREAMS_MASK, syncEnd, count, kind), void
*, dst, const struct cudaArray *, src, size_t, wOffset, size_t, hOffset,
size_t, count, enum cudaMemcpyKind, kind)

CUDA_RUNTIME_SYNC_MEMCPY_WRAPPER(cudaMemcpyArrayToArray, (context,
launcher_cct, syncStart, recorded_node), (context, launcher_cct,
syncStart, recorded_node, ALL_STREAMS_MASK, syncEnd, count, kind),
struct cudaArray *, dst, size_t, wOffsetDst, size_t, hOffsetDst, const
struct cudaArray *, src, size_t, wOffsetSrc, size_t, hOffsetSrc, size_t,
count, enum cudaMemcpyKind, kind)

CUDA_RUNTIME_SYNC_MEMCPY_WRAPPER(cudaMemcpy2DToArray, (context,
launcher_cct, syncStart, recorded_node), (context, launcher_cct,
syncStart, recorded_node, ALL_STREAMS_MASK, syncEnd, (width * height)
, kind), struct cudaArray *, dst, size_t, wOffset, size_t, hOffset,
const void *, src, size_t, spitch, size_t, width, size_t, height, enum
cudaMemcpyKind, kind)

CUDA_RUNTIME_SYNC_MEMCPY_WRAPPER(cudaMemcpy2DFromArray, (context,
launcher_cct, syncStart, recorded_node), (context, launcher_cct,
syncStart, recorded_node, ALL_STREAMS_MASK, syncEnd, (width * height),
kind), void *, dst, size_t, dpitch, const struct cudaArray *, src,
size_t, wOffset, size_t, hOffset, size_t, width, size_t, height, enum
cudaMemcpyKind, kind)

CUDA_RUNTIME_SYNC_MEMCPY_WRAPPER(cudaMemcpy2DArrayToArray, (context,
launcher_cct, syncStart, recorded_node), (context, launcher_cct,
syncStart, recorded_node, ALL_STREAMS_MASK, syncEnd, (width * height),
kind), struct cudaArray *, dst, size_t, wOffsetDst, size_t, hOffsetDst,
const struct cudaArray *, src, size_t, wOffsetSrc, size_t, hOffsetSrc,
size_t, width, size_t, height, enum cudaMemcpyKind, kind )


#if (CUDART_VERSION < 5000)

CUDA_RUNTIME_SYNC_MEMCPY_WRAPPER(cudaMemcpyToSymbol, (context,
launcher_cct, syncStart, recorded_node), (context, launcher_cct,
syncStart, recorded_node, ALL_STREAMS_MASK, syncEnd, count,
kind), const char *, symbol, const void *, src, size_t,
count, size_t, offset , enum cudaMemcpyKind, kind ) 

#else

CUDA_RUNTIME_SYNC_MEMCPY_WRAPPER(cudaMemcpyToSymbol, (context,
launcher_cct, syncStart, recorded_node), (context, launcher_cct,
syncStart, recorded_node, ALL_STREAMS_MASK, syncEnd, count, kind),
const void *, symbol, const void *, src, size_t, count, size_t, offset ,
enum cudaMemcpyKind, kind ) 

#endif


#if (CUDART_VERSION < 5000)

CUDA_RUNTIME_SYNC_MEMCPY_WRAPPER(cudaMemcpyFromSymbol,
(context, launcher_cct, syncStart, recorded_node), (context,
launcher_cct, syncStart, recorded_node, ALL_STREAMS_MASK,
syncEnd, count, kind), void *, dst, const char *, symbol,
size_t, count, size_t, offset , enum cudaMemcpyKind, kind) 

#else

CUDA_RUNTIME_SYNC_MEMCPY_WRAPPER(cudaMemcpyFromSymbol, (context,
launcher_cct, syncStart, recorded_node), (context, launcher_cct,
syncStart, recorded_node, ALL_STREAMS_MASK, syncEnd, count, kind),
void *, dst, const void *, symbol, size_t, count, size_t, offset ,
enum cudaMemcpyKind, kind) 

#endif


CUDA_RUNTIME_ASYNC_MEMCPY_WRAPPER(cudaMemcpyPeerAsync, (streamId,
event_node, context, cct_node, stream, 0), (event_node, cct_node,
stream, count, cudaMemcpyDeviceToDevice), void *, dst, int, dstDevice,
const void *, src, int, srcDevice, size_t, count, cudaStream_t, stream)

CUDA_RUNTIME_ASYNC_MEMCPY_WRAPPER(cudaMemcpyFromArrayAsync, (streamId,
event_node, context, cct_node, stream, 0), (event_node, cct_node,
stream, count, kind), void *, dst, const struct cudaArray *, src,
size_t, wOffset, size_t, hOffset, size_t, count, enum cudaMemcpyKind,
kind, cudaStream_t, stream)

CUDA_RUNTIME_ASYNC_MEMCPY_WRAPPER(cudaMemcpy2DAsync, (streamId,
event_node, context, cct_node, stream, 0), (event_node, cct_node, stream,
(width * height), kind), void *, dst, size_t, dpitch, const void *,
src, size_t, spitch, size_t, width, size_t, height, enum cudaMemcpyKind,
kind, cudaStream_t, stream)

CUDA_RUNTIME_ASYNC_MEMCPY_WRAPPER(cudaMemcpy2DToArrayAsync, (streamId,
event_node, context, cct_node, stream, 0), (event_node, cct_node, stream,
(width * height), kind), struct cudaArray *, dst, size_t, wOffset,
size_t, hOffset, const void *, src, size_t, spitch, size_t, width,
size_t, height, enum cudaMemcpyKind, kind, cudaStream_t, stream)

CUDA_RUNTIME_ASYNC_MEMCPY_WRAPPER(cudaMemcpy2DFromArrayAsync, (streamId,
event_node, context, cct_node, stream, 0), (event_node, cct_node, stream,
(width * height), kind), void *, dst, size_t, dpitch, const struct
cudaArray *, src, size_t, wOffset, size_t, hOffset, size_t, width,
size_t, height, enum cudaMemcpyKind, kind, cudaStream_t, stream)


#if (CUDART_VERSION < 5000)

CUDA_RUNTIME_ASYNC_MEMCPY_WRAPPER(cudaMemcpyToSymbolAsync, (streamId,
event_node, context, cct_node, stream, 0), (event_node, cct_node, stream,
count, kind), const char *, symbol, const void *, src, size_t, count,
size_t, offset, enum cudaMemcpyKind, kind, cudaStream_t, stream)

#else

CUDA_RUNTIME_ASYNC_MEMCPY_WRAPPER(cudaMemcpyToSymbolAsync, (streamId,
event_node, context, cct_node, stream, 0), (event_node, cct_node, stream,
count, kind), const void *, symbol, const void *, src, size_t, count,
size_t, offset, enum cudaMemcpyKind, kind, cudaStream_t, stream)

#endif


#if (CUDART_VERSION < 5000)

CUDA_RUNTIME_ASYNC_MEMCPY_WRAPPER(cudaMemcpyFromSymbolAsync, (streamId,
event_node, context, cct_node, stream, 0), (event_node, cct_node, stream,
count, kind), void *, dst, const char *, symbol, size_t, count, size_t,
offset, enum cudaMemcpyKind, kind, cudaStream_t, stream)

#else

CUDA_RUNTIME_ASYNC_MEMCPY_WRAPPER(cudaMemcpyFromSymbolAsync, (streamId,
event_node, context, cct_node, stream, 0), (event_node, cct_node, stream,
count, kind), void *, dst, const void *, symbol, size_t, count, size_t,
offset, enum cudaMemcpyKind, kind, cudaStream_t, stream)

#endif


CUDA_RUNTIME_SYNC_MEMCPY_WRAPPER(cudaMemset, (context, launcher_cct,
syncStart, recorded_node), (context, launcher_cct, syncStart,
recorded_node, ALL_STREAMS_MASK, syncEnd, 0, cudaMemcpyHostToDevice),
void *, devPtr, int, value, size_t, count)

CUDA_RUNTIME_SYNC_MEMCPY_WRAPPER(cudaMemset2D, (context, launcher_cct,
syncStart, recorded_node), (context, launcher_cct, syncStart,
recorded_node, ALL_STREAMS_MASK, syncEnd, 0, cudaMemcpyHostToDevice),
void *, devPtr, size_t, pitch, int, value, size_t, width, size_t, height)

CUDA_RUNTIME_SYNC_MEMCPY_WRAPPER(cudaMemset3D, (context, launcher_cct,
syncStart, recorded_node), (context, launcher_cct, syncStart,
recorded_node, ALL_STREAMS_MASK, syncEnd, 0, cudaMemcpyHostToDevice),
struct cudaPitchedPtr, pitchedDevPtr, int, value, struct cudaExtent,
extent)

CUDA_RUNTIME_ASYNC_MEMCPY_WRAPPER(cudaMemsetAsync, (streamId, event_node,
context, cct_node, stream, 0), (event_node, cct_node, stream, 0,
cudaMemcpyHostToDevice), void *, devPtr, int, value, size_t, count,
cudaStream_t, stream)

CUDA_RUNTIME_ASYNC_MEMCPY_WRAPPER(cudaMemset2DAsync, (streamId,
event_node, context, cct_node, stream, 0), (event_node, cct_node, stream,
0, cudaMemcpyHostToDevice), void *, devPtr, size_t, pitch, int, value,
size_t, width, size_t, height, cudaStream_t, stream)

CUDA_RUNTIME_ASYNC_MEMCPY_WRAPPER(cudaMemset3DAsync, (streamId,
event_node, context, cct_node, stream, 0), (event_node, cct_node, stream,
0, cudaMemcpyHostToDevice), struct cudaPitchedPtr, pitchedDevPtr, int,
value, struct cudaExtent, extent, cudaStream_t, stream)

CUDA_RUNTIME_ASYNC_MEMCPY_WRAPPER(cudaMemcpyAsync, (streamId, event_node,
context, cct_node, stream, 0), (event_node, cct_node, stream, count,
kind), void *, dst, const void *, src, size_t, count, enum cudaMemcpyKind,
kind, cudaStream_t, stream)

CUDA_RUNTIME_ASYNC_MEMCPY_WRAPPER(cudaMemcpyToArrayAsync, (streamId,
event_node, context, cct_node, stream, 0), (event_node, cct_node,
stream, count, kind), struct cudaArray *, dst, size_t, wOffset, size_t,
hOffset, const void *, src, size_t, count, enum cudaMemcpyKind, kind,
cudaStream_t, stream)

CUDA_RUNTIME_SYNC_MEMCPY_WRAPPER(cudaMemcpy2D, (context, launcher_cct,
syncStart, recorded_node), (context, launcher_cct, syncStart,
recorded_node, ALL_STREAMS_MASK, syncEnd, (height * width), kind), void *,
dst, size_t, dpitch, const void *, src, size_t, spitch, size_t, width,
size_t, height, enum cudaMemcpyKind, kind)

CUDA_RUNTIME_SYNC_MEMCPY_WRAPPER(cudaMemcpy, (context, launcher_cct,
syncStart, recorded_node), (context, launcher_cct, syncStart,
recorded_node, ALL_STREAMS_MASK, syncEnd, count, kind), void *, dst,
const void *, src, size_t, count, enum cudaMemcpyKind, kind)

CUDA_RUNTIME_SYNC_MEMCPY_WRAPPER(cudaMemcpyToArray, (context,
launcher_cct, syncStart, recorded_node), (context, launcher_cct,
syncStart, recorded_node, ALL_STREAMS_MASK, syncEnd, count, kind), struct
cudaArray *, dst, size_t, wOffset, size_t, hOffset, const void *, src,
size_t, count, enum cudaMemcpyKind, kind)


////////////////////////////////////////////////
// CUDA Driver overrides
////////////////////////////////////////////////


CUresult cuStreamSynchronize(CUstream stream) {
    SYNC_PROLOGUE(context, launcher_cct, syncStart, recorded_node);
    
    monitor_disable_new_threads();
    CUresult ret = cuDriverFunctionPointer[cuStreamSynchronizeEnum].cuStreamSynchronizeReal(stream);
    monitor_enable_new_threads();
    
    hpcrun_safe_enter();
    uint32_t streamId;
    streamId = splay_get_stream_id((cudaStream_t)stream);
    hpcrun_safe_exit();
    
    SYNC_EPILOGUE(context, launcher_cct, syncStart, recorded_node, streamId, syncEnd);
    
    return ret;
}


CUresult cuEventSynchronize(CUevent event) {
    SYNC_PROLOGUE(context, launcher_cct, syncStart, recorded_node);
    
    monitor_disable_new_threads();
    CUresult ret = cuDriverFunctionPointer[cuEventSynchronizeEnum].cuEventSynchronizeReal(event);
    monitor_enable_new_threads();
    
    SYNC_EPILOGUE(context, launcher_cct, syncStart, recorded_node, ALL_STREAMS_MASK, syncEnd);
    
    return ret;
}


CUresult cuLaunchGridAsync(CUfunction f, int grid_width, int grid_height, CUstream hStream) {

    ASYNC_KERNEL_PROLOGUE(streamId, event_node, context, cct_node, ((cudaStream_t)hStream), 0);
    
    CUresult ret = cuDriverFunctionPointer[cuLaunchGridAsyncEnum].cuLaunchGridAsyncReal(f, grid_width, grid_height, hStream);

    ASYNC_KERNEL_EPILOGUE(event_node, ((cudaStream_t)hStream));

    return ret;
}

CUresult cuLaunchKernel (CUfunction f,
                                unsigned int gridDimX,
                                unsigned int gridDimY,
                                unsigned int gridDimZ,
                                unsigned int blockDimX,
                                unsigned int blockDimY,
                                unsigned int blockDimZ,
                                unsigned int sharedMemBytes,
                                CUstream hStream,
                                void **kernelParams,
                                void **extra) {
    ASYNC_KERNEL_PROLOGUE(streamId, event_node, context, cct_node, ((cudaStream_t)hStream), 0);

    CUresult ret = cuDriverFunctionPointer[cuLaunchKernelEnum].cuLaunchKernelReal(f, gridDimX, gridDimY, gridDimZ, blockDimX, blockDimY, blockDimZ, sharedMemBytes, hStream, kernelParams, extra);

    ASYNC_KERNEL_EPILOGUE(event_node, ((cudaStream_t)hStream));

    return ret;
}


CUresult cuStreamDestroy(CUstream stream) {
    
    SYNCHRONOUS_CLEANUP;
    hpcrun_safe_enter();
    
    uint32_t streamId;
    streamId = splay_get_stream_id((cudaStream_t)stream);
    
    
    hpcrun_stream_finalize(g_stream_array[streamId].st);

    // remove from hpcrun process auxiliary cleanup list 
    hpcrun_process_aux_cleanup_remove(g_stream_array[streamId].aux_cleanup_info);

    g_stream_array[streamId].st = NULL;
    
    monitor_disable_new_threads();
    cudaError_t ret = cuDriverFunctionPointer[cuStreamDestroy_v2Enum].cuStreamDestroy_v2Real(stream);
    monitor_enable_new_threads();
    
    // Delete splay tree entry
    splay_delete((cudaStream_t)stream);
    hpcrun_safe_exit();
    return ret;
    
}


CUresult cuStreamCreate(CUstream * phStream, unsigned int Flags) {
    
    TD_GET(gpu_data.is_thread_at_cuda_sync) = true;
    monitor_disable_new_threads();
    CUresult ret = cuDriverFunctionPointer[cuStreamCreateEnum].cuStreamCreateReal(phStream, Flags);
    monitor_enable_new_threads();
    TD_GET(gpu_data.is_thread_at_cuda_sync) = false;
    
    StreamCreateBookKeeper((cudaStream_t*) phStream);
    
    return ret;
    
}


static void destroy_all_events_in_free_event_list(){
    
    event_list_node_t * cur = g_free_event_nodes_head;
    
    monitor_disable_new_threads();
    while(cur){
        CUDA_SAFE_CALL(cudaRuntimeFunctionPointer[cudaEventDestroyEnum].cudaEventDestroyReal(cur->event_start));
        CUDA_SAFE_CALL(cudaRuntimeFunctionPointer[cudaEventDestroyEnum].cudaEventDestroyReal(cur->event_end));
        cur->event_start = 0;
        cur->event_end = 0;
        cur = cur->next_free_node;
    }
    monitor_enable_new_threads();
    
}

CUresult
cuCtxCreate_v2 (CUcontext *pctx, unsigned int flags, CUdevice dev)
{
  if (cuda_ncontexts_incr() > 1) {
    fprintf(stderr, "Too many contexts created\n");
    monitor_real_abort();
  }
  if (! hpcrun_is_safe_to_sync(__func__)) {    return cuDriverFunctionPointer[cuCtxCreate_v2Enum].cuCtxCreate_v2Real(pctx, flags, dev);
  }
  TD_GET(gpu_data.is_thread_at_cuda_sync) = true;
  monitor_disable_new_threads();
  CUresult ret = cuDriverFunctionPointer[cuCtxCreate_v2Enum].cuCtxCreate_v2Real(pctx, flags, dev);
  monitor_enable_new_threads();
  TD_GET(gpu_data.is_thread_at_cuda_sync) = false;
  return ret;
}

CUresult cuCtxDestroy(CUcontext ctx) {
    
    SYNCHRONOUS_CLEANUP;
    
    HPCRUN_ASYNC_BLOCK_SPIN_LOCK;
    if (g_start_of_world_time != 0) {
        
        // In case cudaLaunch causes dlopn, async block may get enabled, as a safety net set gpu_data.is_thread_at_cuda_sync so that we dont call any cuda calls
        TD_GET(gpu_data.is_thread_at_cuda_sync) = true;
        
        // Walk the stream splay tree and close each trace.
        close_all_streams(stream_to_id_tree_root);
        stream_to_id_tree_root = NULL;
        
        // And disable tracking new threads from CUDA
        monitor_disable_new_threads();
        
        CUDA_SAFE_CALL(cudaRuntimeFunctionPointer[cudaEventDestroyEnum].cudaEventDestroyReal(g_start_of_world_event));
        g_start_of_world_time = 0;
        // enable monitoring new threads
        monitor_enable_new_threads();
        
        
        // Destroy all events in g_free_event_nodes_head
        destroy_all_events_in_free_event_list();
        
        
        // Ok to call cuda functions from the signal handler
        TD_GET(gpu_data.is_thread_at_cuda_sync) = false;
        
    }
    // count context creation ==> decrement here
    cuda_ncontexts_decr();
    EMSG("Destroying Context!");
    HPCRUN_ASYNC_UNBLOCK_SPIN_UNLOCK;
    
    monitor_disable_new_threads();
    CUresult ret = cuDriverFunctionPointer[cuCtxDestroy_v2Enum].cuCtxDestroy_v2Real(ctx);
    monitor_enable_new_threads();
    
    return ret;
}




CUresult cuMemcpyHtoDAsync(CUdeviceptr dstDevice, const void *srcHost, size_t ByteCount, CUstream hStream) {
    
    ASYNC_MEMCPY_PROLOGUE(streamId, event_node, context, cct_node, ((cudaStream_t)hStream), 0);

    CUresult ret = cuDriverFunctionPointer[cuMemcpyHtoDAsync_v2Enum].cuMemcpyHtoDAsync_v2Real(dstDevice, srcHost, ByteCount, hStream);
    
    ASYNC_MEMCPY_EPILOGUE(event_node, cct_node, ((cudaStream_t)hStream), ByteCount, cudaMemcpyHostToDevice);
    
    return ret;
    
}


CUresult cuMemcpyHtoD(CUdeviceptr dstDevice, const void *srcHost, size_t ByteCount) {
    
    SYNC_MEMCPY_PROLOGUE(context, launcher_cct, syncStart, recorded_node);
    
    monitor_disable_new_threads();
    CUresult ret = cuDriverFunctionPointer[cuMemcpyHtoD_v2Enum].cuMemcpyHtoD_v2Real(dstDevice, srcHost, ByteCount);
    monitor_enable_new_threads();
    
    SYNC_MEMCPY_EPILOGUE(context, launcher_cct, syncStart, recorded_node, ALL_STREAMS_MASK, syncEnd, ByteCount, cudaMemcpyHostToDevice); 
    return ret;
    
}


CUresult cuMemcpyDtoHAsync(void *dstHost, CUdeviceptr srcDevice, size_t ByteCount, CUstream hStream) {
    
    ASYNC_MEMCPY_PROLOGUE(streamId, event_node, context, cct_node, ((cudaStream_t)hStream), 0);
    
    CUresult ret = cuDriverFunctionPointer[cuMemcpyDtoHAsync_v2Enum].cuMemcpyDtoHAsync_v2Real(dstHost, srcDevice, ByteCount, hStream);
    
    ASYNC_MEMCPY_EPILOGUE(event_node, cct_node, ((cudaStream_t)hStream), ByteCount, cudaMemcpyDeviceToHost);
    
    return ret;
}


CUresult cuMemcpyDtoH(void *dstHost, CUdeviceptr srcDevice, size_t ByteCount) {
    
    SYNC_MEMCPY_PROLOGUE(context, launcher_cct, syncStart, recorded_node);
    
    monitor_disable_new_threads();
    CUresult ret = cuDriverFunctionPointer[cuMemcpyDtoH_v2Enum].cuMemcpyDtoH_v2Real(dstHost, srcDevice, ByteCount);
    monitor_enable_new_threads();
    
    SYNC_MEMCPY_EPILOGUE(context, launcher_cct, syncStart, recorded_node, ALL_STREAMS_MASK, syncEnd, ByteCount, cudaMemcpyDeviceToHost); 
    return ret;
}


////////////////////////////////////////////////
// CPU-GPU blame shift interface
////////////////////////////////////////////////


void
gpu_blame_shifter(void* dc, int metric_id, cct_node_t* node,  int metric_dc)
{
  metric_desc_t* metric_desc = hpcrun_id2metric(metric_id);
    
  // Only blame shift idleness for time metric.
  if ( !metric_desc->properties.time )
    return;
    
  uint64_t cur_time_us = 0;
  int ret = time_getTimeReal(&cur_time_us);
  if (ret != 0) {
    EMSG("time_getTimeReal (clock_gettime) failed!");
    monitor_real_abort();
  }
  uint64_t metric_incr = cur_time_us - TD_GET(last_time_us);
    
  // If we are already in a cuda API, then we can't call cleanup_finished_events() since CUDA could have taken the same lock. Hence we just return.
    
  bool is_threads_at_sync = TD_GET(gpu_data.is_thread_at_cuda_sync);
  
  if (is_threads_at_sync) {
    if(SHARED_BLAMING_INITIALISED) {
      TD_GET(gpu_data.accum_num_sync_threads) += ipc_data->num_threads_at_sync_all_procs; 
      TD_GET(gpu_data.accum_num_samples) += 1;
    } 
    return;
  }
    
  spinlock_lock(&g_gpu_lock);
  uint32_t num_unfinshed_streams = 0;
  stream_node_t *unfinished_event_list_head = 0;
    
  num_unfinshed_streams = cleanup_finished_events();
  unfinished_event_list_head = g_unfinished_stream_list_head;
    
  if (num_unfinshed_streams) {
        
    //SHARED BLAMING: kernels need to be blamed for idleness on other procs/threads.
    if(SHARED_BLAMING_INITIALISED && ipc_data->num_threads_at_sync_all_procs && !g_num_threads_at_sync) {
      for (stream_node_t * unfinished_stream = unfinished_event_list_head; unfinished_stream; unfinished_stream = unfinished_stream->next_unfinished_stream) {
	//TODO: FIXME: the local threads at sync need to be removed, /T has to be done while adding metric
	//increment (either one of them).
	cct_metric_data_increment(cpu_idle_cause_metric_id, unfinished_stream->unfinished_event_node->launcher_cct, (cct_metric_data_t) {
	    .r = metric_incr / g_active_threads}
	  );
      }
    }
  }
  else {

    /*** Code to account for Overload factor ***/
    if(TD_GET(gpu_data.overload_state) == WORKING_STATE) {
      TD_GET(gpu_data.overload_state) = OVERLOADABLE_STATE;
    }
        
    if(TD_GET(gpu_data.overload_state) == OVERLOADABLE_STATE) {
      // Increment gpu_overload_potential_metric_id  by metric_incr
      cct_metric_data_increment(gpu_overload_potential_metric_id, node, (cct_metric_data_t) {
	  .i = metric_incr});
    }
        
    // GPU is idle iff   ipc_data->outstanding_kernels == 0 
    // If ipc_data is NULL, then this process has not made GPU calls so, we are blind and declare GPU idle w/o checking status of other processes
    // There is no better solution yet since we dont know which GPU card we should be looking for idleness. 
    if(g_do_shared_blaming){
      if ( !ipc_data || ipc_data->outstanding_kernels == 0) { // GPU device is truely idle i.e. no other process is keeping it busy
	// Increment gpu_ilde by metric_incr
	cct_metric_data_increment(gpu_idle_metric_id, node, (cct_metric_data_t) {
	    .i = metric_incr});
      }
    } else {
      // Increment gpu_ilde by metric_incr
      cct_metric_data_increment(gpu_idle_metric_id, node, (cct_metric_data_t) {
	  .i = metric_incr});            
    }
        
  }
  spinlock_unlock(&g_gpu_lock);
}

#endif
