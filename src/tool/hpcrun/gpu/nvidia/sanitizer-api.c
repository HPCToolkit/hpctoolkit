// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2019, Rice University
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

//***************************************************************************
//
// File:
//   sanitizer-api.c
//
// Purpose:
//   implementation of wrapper around NVIDIA's Sanitizer API
//  
//***************************************************************************

//***************************************************************************
// system includes
//***************************************************************************

#include <stdio.h>
#include <errno.h>     // errno
#include <fcntl.h>     // open
#include <limits.h>    // PATH_MAX
#include <stdio.h>     // sprintf
#include <unistd.h>
#include <sys/stat.h>  // mkdir
#include <string.h>    // strstr
#include <pthread.h>
#include <time.h>

#ifndef HPCRUN_STATIC_LINK
#include <dlfcn.h>
#undef _GNU_SOURCE
#define _GNU_SOURCE
#include <link.h>          // dl_iterate_phdr
#include <linux/limits.h>  // PATH_MAX
#include <string.h>        // strstr
#endif

#include <sanitizer.h>
#include <gpu-patch.h>
#include <redshow.h>
#include <vector_types.h>  // dim3

#include <lib/prof-lean/spinlock.h>
#include <lib/prof-lean/stdatomic.h>

#include <hpcrun/cct2metrics.h>
#include <hpcrun/files.h>
#include <hpcrun/hpcrun_stats.h>
#include <hpcrun/module-ignore-map.h>
#include <hpcrun/main.h>
#include <hpcrun/safe-sampling.h>
#include <hpcrun/sample_event.h>
#include <hpcrun/thread_data.h>
#include <hpcrun/threadmgr.h>

#include <hpcrun/gpu/gpu-application-thread-api.h>
#include <hpcrun/gpu/gpu-monitoring-thread-api.h>
#include <hpcrun/gpu/gpu-correlation-id.h>
#include <hpcrun/gpu/gpu-op-placeholders.h>
#include <hpcrun/gpu/gpu-metrics.h>

#include <hpcrun/sample-sources/libdl.h>
#include <hpcrun/sample-sources/nvidia.h>

#include "cuda-api.h"
#include "cubin-id-map.h"
#include "cubin-hash-map.h"
#include "sanitizer-api.h"
#include "sanitizer-context-map.h"
#include "sanitizer-stream-map.h"
#include "sanitizer-op-map.h"
#include "sanitizer-buffer.h"
#include "sanitizer-buffer-channel.h"
#include "sanitizer-buffer-channel-set.h"
#include "sanitizer-function-list.h"

#define SANITIZER_API_DEBUG 1

#define FUNCTION_NAME_LENGTH 1024

#if SANITIZER_API_DEBUG
#define PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define PRINT(...)
#endif

#define MIN2(m1, m2) m1 > m2 ? m2 : m1

#define SANITIZER_FN_NAME(f) DYN_FN_NAME(f)

#define SANITIZER_FN(fn, args) \
  static SanitizerResult (*SANITIZER_FN_NAME(fn)) args

#define HPCRUN_SANITIZER_CALL(fn, args) \
{ \
  SanitizerResult status = SANITIZER_FN_NAME(fn) args; \
  if (status != SANITIZER_SUCCESS) { \
    sanitizer_error_report(status, #fn); \
  }	\
}

#define HPCRUN_SANITIZER_CALL_NO_CHECK(fn, args) \
{ \
  SANITIZER_FN_NAME(fn) args; \
}

#define DISPATCH_CALLBACK(fn, args) if (fn) fn args

#define FORALL_SANITIZER_ROUTINES(macro)   \
  macro(sanitizerSubscribe)                \
  macro(sanitizerUnsubscribe)              \
  macro(sanitizerEnableAllDomains)         \
  macro(sanitizerEnableDomain)             \
  macro(sanitizerAlloc)                    \
  macro(sanitizerMemset)                   \
  macro(sanitizerMemcpyDeviceToHost)       \
  macro(sanitizerMemcpyHostToDeviceAsync)  \
  macro(sanitizerSetCallbackData)          \
  macro(sanitizerStreamSynchronize)        \
  macro(sanitizerAddPatchesFromFile)       \
  macro(sanitizerGetFunctionPcAndSize)     \
  macro(sanitizerPatchInstructions)        \
  macro(sanitizerPatchModule)              \
  macro(sanitizerGetResultString)          \
  macro(sanitizerUnpatchModule)


typedef void (*sanitizer_error_callback_t)
(
 const char *type,
 const char *fn,
 const char *error_string
);

typedef cct_node_t *(*sanitizer_correlation_callback_t)
(
 uint64_t id,
 uint32_t skip_frames
);

static cct_node_t *
sanitizer_correlation_callback_dummy // __attribute__((unused))
(
 uint64_t id,
 uint32_t skip_frames
);

static void
sanitizer_error_callback_dummy // __attribute__((unused))
(
 const char *type,
 const char *fn,
 const char *error_string
);

typedef struct {
  pthread_t thread;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
} sanitizer_thread_t;

// only subscribed by the main thread
static Sanitizer_SubscriberHandle sanitizer_subscriber_handle;
// Single background process thread, can be extended
static sanitizer_thread_t sanitizer_thread;

// Configurable variables
static int sanitizer_gpu_patch_record_num = 0;
static int sanitizer_buffer_pool_size = 0;
static int sanitizer_pc_views = 0;
static int sanitizer_mem_views = 0;

static redshow_approx_level_t sanitizer_approx_level = REDSHOW_APPROX_NONE;
static redshow_data_type_t sanitizer_data_type = REDSHOW_DATA_FLOAT;

static bool sanitizer_analysis_async = false;

static __thread bool sanitizer_stop_flag = false;
static __thread bool sanitizer_internal_flag = false;
static __thread uint32_t sanitizer_thread_id_self = (1 << 30);
static __thread uint32_t sanitizer_thread_id_local = 0;
static __thread CUcontext sanitizer_thread_context = NULL;

static __thread gpu_patch_buffer_t gpu_patch_buffer_reset = {
  .head_index = 0,
  .tail_index = 0,
  .size = 0,
  .full = 0
};
static __thread gpu_patch_buffer_t *gpu_patch_buffer_device = NULL;
static __thread gpu_patch_buffer_t *gpu_patch_buffer_host = NULL;

static sanitizer_correlation_callback_t sanitizer_correlation_callback =
  sanitizer_correlation_callback_dummy;

static sanitizer_error_callback_t sanitizer_error_callback =
  sanitizer_error_callback_dummy;

static atomic_uint sanitizer_thread_id = ATOMIC_VAR_INIT(0);
static atomic_uint sanitizer_process_thread_counter = ATOMIC_VAR_INIT(0);
static atomic_bool sanitizer_process_awake_flag = ATOMIC_VAR_INIT(0);
static atomic_bool sanitizer_process_stop_flag = ATOMIC_VAR_INIT(0);

static spinlock_t sanitizer_alloc_lock = SPINLOCK_UNLOCKED;
static spinlock_t sanitizer_free_lock = SPINLOCK_UNLOCKED;

static sanitizer_function_list_entry_t *sanitizer_whitelist = NULL;
static sanitizer_function_list_entry_t *sanitizer_blacklist = NULL;

//----------------------------------------------------------
// sanitizer function pointers for late binding
//----------------------------------------------------------

SANITIZER_FN
(
 sanitizerSubscribe,
 (
  Sanitizer_SubscriberHandle* subscriber,
  Sanitizer_CallbackFunc callback,
  void* userdata
 )
);


SANITIZER_FN
(
 sanitizerUnsubscribe,
 (
  Sanitizer_SubscriberHandle subscriber
 )
);


SANITIZER_FN
(
 __attribute__((unused)) sanitizerEnableAllDomains,
 (
  uint32_t enable,
  Sanitizer_SubscriberHandle subscriber
 ) 
);


SANITIZER_FN
(
 sanitizerEnableDomain,
 (
  uint32_t enable,
  Sanitizer_SubscriberHandle subscriber,
  Sanitizer_CallbackDomain domain
 ) 
);


SANITIZER_FN
(
 sanitizerAlloc,
 (
  CUcontext ctx,
  void** devPtr,
  size_t size
 )
);


SANITIZER_FN
(
 sanitizerMemset,
 (
  void* devPtr,
  int value,
  size_t count,
  Sanitizer_StreamHandle stream
 )
);


SANITIZER_FN
(
 sanitizerStreamSynchronize,
 (
  Sanitizer_StreamHandle stream
 )
);


SANITIZER_FN
(
 sanitizerMemcpyDeviceToHost,
 (
  void* dst,
  void* src,
  size_t count,
  Sanitizer_StreamHandle stream
 ) 
);


SANITIZER_FN
(
 sanitizerMemcpyHostToDeviceAsync,
 (
  void* dst,
  void* src,
  size_t count,
  Sanitizer_StreamHandle stream
 ) 
);


SANITIZER_FN
(
 sanitizerSetCallbackData,
 (
  CUfunction function,
  const void* userdata
 ) 
);


SANITIZER_FN
(
 sanitizerAddPatchesFromFile,
 (
  const char* filename,
  CUcontext ctx
 ) 
);


SANITIZER_FN
(
 sanitizerPatchInstructions,
 (
  const Sanitizer_InstructionId instructionId,
  CUmodule module,
  const char* deviceCallbackName
 ) 
);


SANITIZER_FN
(
 sanitizerPatchModule,
 (
  CUmodule module
 )
);


SANITIZER_FN
(
 __attribute__((unused)) sanitizerUnpatchModule,
 (
  CUmodule module
 )
);


SANITIZER_FN
(
 sanitizerGetResultString,
 (
  SanitizerResult result,
  const char **str
 )
);


SANITIZER_FN
(
 sanitizerGetFunctionPcAndSize,
 (
  CUmodule module,
  const char *functionName,
  uint64_t* pc,
  uint64_t* size
 )
);

//******************************************************************************
// forward declaration operations
//******************************************************************************

static Sanitizer_StreamHandle sanitizer_priority_stream_get(CUcontext context);

//******************************************************************************
// private operations
//******************************************************************************


static cct_node_t *
sanitizer_correlation_callback_dummy // __attribute__((unused))
(
 uint64_t id,
 uint32_t skip_frames
)
{
  return NULL;
}


static void
sanitizer_error_callback_dummy // __attribute__((unused))
(
 const char *type,
 const char *fn,
 const char *error_string
)
{
  PRINT("%s: function %s failed with error %s\n", type, fn, error_string);
  exit(-1);
}


static void
sanitizer_error_report
(
 SanitizerResult error,
 const char *fn
)
{
  const char *error_string;
  SANITIZER_FN_NAME(sanitizerGetResultString)(error, &error_string);
  sanitizer_error_callback("Sanitizer result error", fn, error_string);
}


static void
sanitizer_log_data_callback
(
 int32_t kernel_id,
 gpu_patch_buffer_t *trace_data
)
{
}


static void
sanitizer_dtoh
(
 uint64_t host,
 uint64_t device,
 uint64_t len
)
{
  Sanitizer_StreamHandle priority_stream = sanitizer_priority_stream_get(sanitizer_thread_context);

  HPCRUN_SANITIZER_CALL(sanitizerMemcpyDeviceToHost,
    ((void *)host, (void *)device, len, priority_stream));
}


static void
sanitizer_record_data_callback
(
 uint32_t cubin_id,
 int32_t kernel_id,
 redshow_record_data_t *record_data
)
{
  gpu_activity_t ga;

  ga.kind = GPU_ACTIVITY_REDUNDANCY;

  if (record_data->analysis_type == REDSHOW_ANALYSIS_SPATIAL_REDUNDANCY &&
    record_data->access_type == REDSHOW_ACCESS_READ) {
    ga.details.redundancy.type = GPU_RED_SPATIAL_READ_RED;
  } else if (record_data->analysis_type == REDSHOW_ANALYSIS_SPATIAL_REDUNDANCY &&
    record_data->access_type == REDSHOW_ACCESS_WRITE) {
    ga.details.redundancy.type = GPU_RED_SPATIAL_WRITE_RED;
  } else if (record_data->analysis_type == REDSHOW_ANALYSIS_TEMPORAL_REDUNDANCY &&
    record_data->access_type == REDSHOW_ACCESS_READ) {
    ga.details.redundancy.type = GPU_RED_TEMPORAL_READ_RED;
  } else if (record_data->analysis_type == REDSHOW_ANALYSIS_TEMPORAL_REDUNDANCY &&
    record_data->access_type == REDSHOW_ACCESS_WRITE) {
    ga.details.redundancy.type = GPU_RED_TEMPORAL_WRITE_RED;
  } else {
    assert(0);
  }
  cstack_ptr_set(&(ga.next), 0);
  
  uint32_t num_views = record_data->num_views;
  uint32_t i;
  for (i = 0; i < num_views; ++i) {
    uint32_t function_index = record_data->views[i].function_index;
    uint64_t pc_offset = record_data->views[i].pc_offset;
    uint64_t red_count = record_data->views[i].red_count;
    uint64_t access_count = record_data->views[i].access_count;

    ip_normalized_t ip = cubin_id_transform(cubin_id, function_index, pc_offset);
    sanitizer_op_map_entry_t *entry = sanitizer_op_map_lookup(kernel_id);
    if (entry != NULL) {
      cct_node_t *host_op_node = sanitizer_op_map_op_get(entry);
      ga.cct_node = hpcrun_cct_insert_ip_norm(host_op_node, ip);
      ga.details.redundancy.red_count = red_count;
      ga.details.redundancy.access_count = access_count;
      // Associate record_data with calling context (kernel_id)
      gpu_metrics_attribute(&ga);
    } else {
      PRINT("NULL cct_node with kernel_id %d\n", kernel_id);
    }
  }
}



#ifndef HPCRUN_STATIC_LINK
static const char *
sanitizer_path
(
 void
)
{
  const char *path = "libsanitizer-public.so";
    
  static char buffer[PATH_MAX];
  buffer[0] = 0;

  // open an NVIDIA library to find the CUDA path with dl_iterate_phdr
  // note: a version of this file with a more specific name may 
  // already be loaded. thus, even if the dlopen fails, we search with
  // dl_iterate_phdr.
  void *h = monitor_real_dlopen("libcudart.so", RTLD_LOCAL | RTLD_LAZY);

  if (cuda_path(buffer)) {
    // invariant: buffer contains CUDA home 
    strcat(buffer, "Sanitizer/libsanitizer-public.so");
    path = buffer;
  }

  if (h) monitor_real_dlclose(h);

  return path;
}

#endif

//******************************************************************************
// asynchronous process thread
//******************************************************************************

void
sanitizer_process_signal
(
)
{
  pthread_cond_t *cond = &(sanitizer_thread.cond);
  pthread_mutex_t *mutex = &(sanitizer_thread.mutex);

  pthread_mutex_lock(mutex);

  atomic_store(&sanitizer_process_awake_flag, true);

  pthread_cond_signal(cond);

  pthread_mutex_unlock(mutex);
}


static void
sanitizer_process_await
(
)
{
  pthread_cond_t *cond = &(sanitizer_thread.cond);
  pthread_mutex_t *mutex = &(sanitizer_thread.mutex);

  pthread_mutex_lock(mutex);

  while (!atomic_load(&sanitizer_process_awake_flag)) {
    pthread_cond_wait(cond, mutex);
  }

  atomic_store(&sanitizer_process_awake_flag, false);

  pthread_mutex_unlock(mutex);
}


static void*
sanitizer_process_thread
(
 void *arg
)
{
  pthread_cond_t *cond = &(sanitizer_thread.cond);
  pthread_mutex_t *mutex = &(sanitizer_thread.mutex);

  while (!atomic_load(&sanitizer_process_stop_flag)) {
    redshow_analysis_begin();
    sanitizer_buffer_channel_set_consume();
    redshow_analysis_end();
    sanitizer_process_await();
  }

  // Last records
  sanitizer_buffer_channel_set_consume();

  // Create thread data
  thread_data_t* td = NULL;
  int id = sanitizer_thread_id_self;
  hpcrun_threadMgr_non_compact_data_get(id, NULL, &td);
  hpcrun_set_thread_data(td);
  
  atomic_fetch_add(&sanitizer_process_thread_counter, -1);

  pthread_mutex_destroy(mutex);
  pthread_cond_destroy(cond);

  return NULL;
}

//******************************************************************************
// Cubins
//******************************************************************************


static void
sanitizer_load_callback
(
 CUcontext context,
 CUmodule module, 
 const void *cubin, 
 size_t cubin_size
)
{
  hpctoolkit_cumod_st_t *cumod = (hpctoolkit_cumod_st_t *)module;
  uint32_t cubin_id = cumod->cubin_id;
  uint32_t mod_id = cumod->mod_id;

  // Compute hash for cubin and store it into a map
  cubin_hash_map_entry_t *cubin_hash_entry = cubin_hash_map_lookup(cubin_id);
  unsigned char *hash;
  unsigned int hash_len;
  if (cubin_hash_entry == NULL) {
    cubin_hash_map_insert(cubin_id, cubin, cubin_size);
    cubin_hash_entry = cubin_hash_map_lookup(cubin_id);
  }
  hash = cubin_hash_map_entry_hash_get(cubin_hash_entry, &hash_len);

  // Create file name
  char file_name[PATH_MAX];
  size_t i;
  size_t used = 0;
  used += sprintf(&file_name[used], "%s", hpcrun_files_output_directory());
  used += sprintf(&file_name[used], "%s", "/cubins/");
  mkdir(file_name, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  for (i = 0; i < hash_len; ++i) {
    used += sprintf(&file_name[used], "%02x", hash[i]);
  }
  used += sprintf(&file_name[used], "%s", ".cubin");
  PRINT("cubin_id %d hash %s\n", cubin_id, file_name);

  uint32_t hpctoolkit_module_id;
  load_module_t *load_module = NULL;
  hpcrun_loadmap_lock();
  if ((load_module = hpcrun_loadmap_findByName(file_name)) == NULL) {
    hpctoolkit_module_id = hpcrun_loadModule_add(file_name);
  } else {
    hpctoolkit_module_id = load_module->id;
  }
  hpcrun_loadmap_unlock();
  PRINT("<cubin_id %d, mod_id %d> -> hpctoolkit_module_id %d\n", cubin_id, mod_id, hpctoolkit_module_id);

  // Compute elf vector
  Elf_SymbolVector *elf_vector = computeCubinFunctionOffsets(cubin, cubin_size);

  // Register cubin module
  cubin_id_map_insert(cubin_id, hpctoolkit_module_id, elf_vector);

  // Query cubin function offsets
  uint64_t *addrs = (uint64_t *)hpcrun_malloc_safe(sizeof(uint64_t) * elf_vector->nsymbols);
  for (i = 0; i < elf_vector->nsymbols; ++i) {
    addrs[i] = 0;
    if (elf_vector->symbols[i] != 0) {
      uint64_t pc;
      uint64_t size;
      // do not check error
      HPCRUN_SANITIZER_CALL_NO_CHECK(sanitizerGetFunctionPcAndSize, (module, elf_vector->names[i], &pc, &size));
      addrs[i] = pc;
    }
  }
  redshow_cubin_cache_register(cubin_id, mod_id, elf_vector->nsymbols, addrs, file_name);

  PRINT("Patch CUBIN: \n");
  PRINT("%s\n", HPCTOOLKIT_GPU_PATCH);
  // patch binary
  HPCRUN_SANITIZER_CALL(sanitizerAddPatchesFromFile, (HPCTOOLKIT_GPU_PATCH, context));
  HPCRUN_SANITIZER_CALL(sanitizerPatchInstructions,
    (SANITIZER_INSTRUCTION_MEMORY_ACCESS, module, "sanitizer_memory_access_callback"));
  HPCRUN_SANITIZER_CALL(sanitizerPatchInstructions,
    (SANITIZER_INSTRUCTION_BLOCK_ENTER, module, "sanitizer_block_enter_callback"));
  HPCRUN_SANITIZER_CALL(sanitizerPatchInstructions,
    (SANITIZER_INSTRUCTION_BLOCK_EXIT, module, "sanitizer_block_exit_callback"));
  HPCRUN_SANITIZER_CALL(sanitizerPatchModule, (module));
}


static void
sanitizer_unload_callback
(
 const void *module,
 const void *cubin, 
 size_t cubin_size
)
{
  hpctoolkit_cumod_st_t *cumod = (hpctoolkit_cumod_st_t *)module;
  cuda_unload_callback(cumod->cubin_id);

  // We cannot unregister cubins
  //redshow_cubin_unregister(cumod->cubin_id, cumod->mod_id);
}

//******************************************************************************
// record handlers
//******************************************************************************

static void __attribute__((unused))
dim3_id_transform
(
 dim3 dim,
 uint32_t flat_id,
 uint32_t *id_x,
 uint32_t *id_y,
 uint32_t *id_z
)
{
  *id_z = flat_id / (dim.x * dim.y);
  *id_y = (flat_id - (*id_z) * dim.x * dim.y) / (dim.x);
  *id_x = (flat_id - (*id_z) * dim.x * dim.y - (*id_y) * dim.x);
}


static Sanitizer_StreamHandle
sanitizer_priority_stream_get
(
 CUcontext context
)
{
  sanitizer_context_map_entry_t *entry = sanitizer_context_map_init(context);

  Sanitizer_StreamHandle priority_stream =
    (Sanitizer_StreamHandle)sanitizer_context_map_entry_priority_stream_get(entry);

  if (priority_stream == NULL) {
    // First time
    sanitizer_internal_flag = true;
    // Sanitizer callback will associate a priority stream
    cuda_priority_stream_create();
    sanitizer_internal_flag = false;
    priority_stream = (Sanitizer_StreamHandle)sanitizer_context_map_entry_priority_stream_get(entry);
  }

  return priority_stream;
}


static void
sanitizer_kernel_launch_sync
(
 cct_node_t *api_node,
 uint64_t correlation_id,
 CUcontext context,
 Sanitizer_StreamHandle handle_stream,
 CUmodule module,
 CUfunction function,
 Sanitizer_StreamHandle priority_stream,
 dim3 grid_size,
 dim3 block_size
)
{
  // Look up module id
  hpctoolkit_cumod_st_t *cumod = (hpctoolkit_cumod_st_t *)module;
  uint32_t cubin_id = cumod->cubin_id;
  uint32_t mod_id = cumod->mod_id;

  // TODO(Keren): correlate metrics with api_node

  int block_sampling_frequency = sanitizer_block_sampling_frequency_get();
  int grid_dim = grid_size.x * grid_size.y * grid_size.z;
  int block_dim = block_size.x * block_size.y * block_size.z;
  uint64_t num_threads = grid_dim * block_dim;
  size_t num_left_threads = 0;

  // If block sampling is set
  if (block_sampling_frequency != 0) {
    // Uniform sampling
    int sampling_offset = gpu_patch_buffer_reset.block_sampling_offset;
    int mod_blocks = grid_dim % block_sampling_frequency;
    int sampling_blocks = 0;
    if (mod_blocks == 0) {
      sampling_blocks = (grid_dim - 1) / block_sampling_frequency + 1;
    } else {
      sampling_blocks = (grid_dim - 1) / block_sampling_frequency + (sampling_offset >= mod_blocks ? 0 : 1);
    }
    num_left_threads = num_threads - sampling_blocks * block_dim;
  }

  // Init a buffer on host
  if (gpu_patch_buffer_host == NULL) {
    gpu_patch_buffer_host = (gpu_patch_buffer_t *)hpcrun_malloc_safe(sizeof(gpu_patch_buffer_t));
  }

  // Reserve for debugging correctness
  //PRINT("head_index %u, tail_index %u, num_left_threads %lu\n",
  //  gpu_patch_buffer_host->head_index, gpu_patch_buffer_host->tail_index, num_threads);

  while (true) {
    // Copy buffer
    HPCRUN_SANITIZER_CALL(sanitizerMemcpyDeviceToHost,
      (gpu_patch_buffer_host, gpu_patch_buffer_device, sizeof(gpu_patch_buffer_t), priority_stream));

    size_t num_records = gpu_patch_buffer_host->head_index;

    // Reserve for debugging correctness
    //PRINT("head_index %u, tail_index %u, num_left_threads %u expected %zu\n",
    //  gpu_patch_buffer_host->head_index, gpu_patch_buffer_host->tail_index, gpu_patch_buffer_host->num_threads, num_left_threads);

    // Wait until the buffer is full or the kernel is finished
    if (!(gpu_patch_buffer_host->num_threads == num_left_threads || gpu_patch_buffer_host->full) || num_records == 0) {
      continue;
    }

    // Reserve for debugging correctness
    //PRINT("num_records %zu\n", num_records);

    // Allocate memory
    int32_t persistent_id = hpcrun_cct_persistent_id(api_node);
    sanitizer_buffer_t *sanitizer_buffer = sanitizer_buffer_channel_produce(
      sanitizer_thread_id_local, cubin_id, mod_id, persistent_id, correlation_id,
      sanitizer_gpu_patch_record_num, sanitizer_analysis_async);
    gpu_patch_buffer_t *gpu_patch_buffer = sanitizer_buffer_entry_gpu_patch_buffer_get(sanitizer_buffer);

    // If sync mode and not enough buffer, empty current buffer
    if (gpu_patch_buffer == NULL && !sanitizer_analysis_async) {
      sanitizer_buffer_channel_t *channel = sanitizer_buffer_channel_get();
      sanitizer_buffer_channel_consume(channel);
      // Get it again
      sanitizer_buffer = sanitizer_buffer_channel_produce(
        sanitizer_thread_id_local, cubin_id, mod_id, persistent_id, correlation_id,
        sanitizer_gpu_patch_record_num, sanitizer_analysis_async);
      gpu_patch_buffer = sanitizer_buffer_entry_gpu_patch_buffer_get(sanitizer_buffer);
    }

    // Move host buffer to a cache
    memcpy(gpu_patch_buffer, gpu_patch_buffer_host, offsetof(gpu_patch_buffer_t, records));
    // Copy all records to the cache
    gpu_patch_record_t *gpu_patch_record_device = (gpu_patch_record_t *)gpu_patch_buffer_host->records;

    HPCRUN_SANITIZER_CALL(sanitizerMemcpyDeviceToHost,
      (gpu_patch_buffer->records, gpu_patch_record_device, sizeof(gpu_patch_record_t) * num_records, priority_stream));

    // Tell gpu copy is finished
    gpu_patch_buffer_host->full = 0;
    // Do not need to sync stream.
    // The function will return once the pageable buffer has been copied to the staging memory.
    // for DMA transfer to device memory, but the DMA to final destination may not have completed.
    // Only copy the first field because other fields are being updated by the GPU.
    HPCRUN_SANITIZER_CALL(sanitizerMemcpyHostToDeviceAsync,
      (gpu_patch_buffer_device, gpu_patch_buffer_host, sizeof(gpu_patch_buffer_host->full), priority_stream));
    
    sanitizer_buffer_channel_push(sanitizer_buffer);

    // Awake background thread
    if (sanitizer_analysis_async) {
      // If multiple application threads are created, it might miss a signal,
      // but we finally still process all the records
      sanitizer_process_signal(); 
    }
    
    // Finish all the threads
    if (gpu_patch_buffer->num_threads == num_left_threads) {
      break;
    }
  }

  // To ensure previous kernel is done
  HPCRUN_SANITIZER_CALL(sanitizerStreamSynchronize, (handle_stream));

  if (!sanitizer_analysis_async) {
    // Empty current buffer
    sanitizer_buffer_channel_t *channel = sanitizer_buffer_channel_get();
    sanitizer_buffer_channel_consume(channel);
  }
}

//******************************************************************************
// callbacks
//******************************************************************************

static void
sanitizer_kernel_launch_callback
(
 CUcontext context,
 Sanitizer_StreamHandle handle_stream,
 CUfunction function,
 dim3 grid_size,
 dim3 block_size,
 bool kernel_sampling
)
{
  int grid_dim = grid_size.x * grid_size.y * grid_size.z;
  int block_dim = block_size.x * block_size.y * block_size.z;
  int block_sampling_frequency = kernel_sampling ? sanitizer_block_sampling_frequency_get() : 0;
  int block_sampling_offset = kernel_sampling ? rand() % grid_dim % block_sampling_frequency : 0;

  PRINT("Kernel sampling %d\n", kernel_sampling);
  PRINT("Sampling offset %d\n", block_sampling_offset);
  PRINT("Sampling frequency %d\n", block_sampling_frequency);

  if (gpu_patch_buffer_device == NULL) {
    // allocate buffer
    void *gpu_patch_records = NULL;

    // gpu_patch_buffer
    HPCRUN_SANITIZER_CALL(sanitizerAlloc, (context, (void **)(&(gpu_patch_buffer_device)), sizeof(gpu_patch_buffer_t)));
    HPCRUN_SANITIZER_CALL(sanitizerMemset, (gpu_patch_buffer_device, 0, sizeof(gpu_patch_buffer_t), handle_stream));

    PRINT("Allocate gpu_patch_buffer %p, size %zu\n", gpu_patch_buffer_device, sizeof(gpu_patch_buffer_t));

    // gpu_patch_buffer_t->records
    HPCRUN_SANITIZER_CALL(sanitizerAlloc,
      (context, &gpu_patch_records, sanitizer_gpu_patch_record_num * sizeof(gpu_patch_record_t)));
    HPCRUN_SANITIZER_CALL(sanitizerMemset,
      (gpu_patch_records, 0, sanitizer_gpu_patch_record_num * sizeof(gpu_patch_record_t), handle_stream));

    PRINT("Allocate gpu_patch_records %p, size %zu\n", gpu_patch_records, sanitizer_gpu_patch_record_num * sizeof(gpu_patch_record_t));

    gpu_patch_buffer_reset.records = gpu_patch_records;
    gpu_patch_buffer_reset.num_threads = grid_dim * block_dim;
    gpu_patch_buffer_reset.block_sampling_frequency = block_sampling_frequency;
    gpu_patch_buffer_reset.block_sampling_offset = block_sampling_offset;
    gpu_patch_buffer_reset.size = sanitizer_gpu_patch_record_num;

    HPCRUN_SANITIZER_CALL(sanitizerMemcpyHostToDeviceAsync,
      (gpu_patch_buffer_device, &gpu_patch_buffer_reset, sizeof(gpu_patch_buffer_t), handle_stream));
  } else {
    // reset buffer
    gpu_patch_buffer_reset.num_threads = grid_dim * block_dim;
    gpu_patch_buffer_reset.block_sampling_frequency = block_sampling_frequency;
    gpu_patch_buffer_reset.block_sampling_offset = block_sampling_offset;

    HPCRUN_SANITIZER_CALL(sanitizerMemcpyHostToDeviceAsync,
      (gpu_patch_buffer_device, &gpu_patch_buffer_reset,
       sizeof(gpu_patch_buffer_t), handle_stream));
  }

  HPCRUN_SANITIZER_CALL(sanitizerSetCallbackData, (function, gpu_patch_buffer_device));

  // To ensure previous copies are done
  HPCRUN_SANITIZER_CALL(sanitizerStreamSynchronize, (handle_stream));
}

//-------------------------------------------------------------
// callback controls
//-------------------------------------------------------------

static void
sanitizer_subscribe_callback
(
 void* userdata,
 Sanitizer_CallbackDomain domain,
 Sanitizer_CallbackId cbid,
 const void* cbdata
)
{
  if (!sanitizer_stop_flag) {
    sanitizer_thread_id_local = atomic_fetch_add(&sanitizer_thread_id, 1);
    sanitizer_stop_flag_set();
  }

  // XXX(keren): assume single thread per stream
  if (domain == SANITIZER_CB_DOMAIN_RESOURCE) {
    switch (cbid) {
      case SANITIZER_CBID_RESOURCE_MODULE_LOADED:
        {
          // single thread
          Sanitizer_ResourceModuleData *md = (Sanitizer_ResourceModuleData *)cbdata;
          sanitizer_thread_context = md->context;

          sanitizer_load_callback(md->context, md->module, md->pCubin, md->cubinSize);
          break;
        }
      case SANITIZER_CBID_RESOURCE_MODULE_UNLOAD_STARTING:
        {
          // single thread
          Sanitizer_ResourceModuleData *md = (Sanitizer_ResourceModuleData *)cbdata;
          sanitizer_thread_context = md->context;

          sanitizer_unload_callback(md->module, md->pCubin, md->cubinSize);
          break;
        }
      case SANITIZER_CBID_RESOURCE_STREAM_CREATED:
        {
          // single thread
          Sanitizer_ResourceStreamData *sd = (Sanitizer_ResourceStreamData *)cbdata;
          sanitizer_thread_context = sd->context;

          if (sanitizer_internal_flag) {
            // Update priority stream
            sanitizer_context_map_priority_stream_update(sd->context, sd->hStream);
          }
          break;
        }
      case SANITIZER_CBID_RESOURCE_STREAM_DESTROY_STARTING:
        {
          // single thread
          // TODO
          PRINT("Stream destroy starting\n");
          break;
        }
      case SANITIZER_CBID_RESOURCE_CONTEXT_CREATION_FINISHED:
        {
          break;
        }
      case SANITIZER_CBID_RESOURCE_CONTEXT_DESTROY_STARTING:
        {
          // single thread
          // TODO
          PRINT("Context destroy starting\n");
          break;
        }
      case SANITIZER_CBID_RESOURCE_DEVICE_MEMORY_ALLOC:
        {
          Sanitizer_ResourceMemoryData *md = (Sanitizer_ResourceMemoryData *)cbdata;
          sanitizer_thread_context = md->context;

          spinlock_lock(&sanitizer_alloc_lock);

          uint64_t correlation_id = gpu_correlation_id();
          cct_node_t *api_node = sanitizer_correlation_callback(correlation_id, 0);

          hpcrun_safe_enter();

          gpu_op_ccts_t gpu_op_ccts;
          gpu_op_placeholder_flags_t gpu_op_placeholder_flags = 0;
          gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags, 
            gpu_placeholder_type_alloc);
          gpu_op_ccts_insert(api_node, &gpu_op_ccts, gpu_op_placeholder_flags);
          api_node = gpu_op_ccts_get(&gpu_op_ccts, gpu_placeholder_type_alloc);

          hpcrun_safe_exit();

          int32_t persistent_id = hpcrun_cct_persistent_id(api_node);
          redshow_memory_register(persistent_id, correlation_id, md->address, md->address + md->size);

          PRINT("Allocate memory address %p, size %zu, op %lu, id %d\n",
            (void *)md->address, md->size, correlation_id, persistent_id);

          spinlock_unlock(&sanitizer_alloc_lock);
          break;
        }
      case SANITIZER_CBID_RESOURCE_DEVICE_MEMORY_FREE:
        {
          Sanitizer_ResourceMemoryData *md = (Sanitizer_ResourceMemoryData *)cbdata;
          sanitizer_thread_context = md->context;

          spinlock_lock(&sanitizer_free_lock);
          
          uint64_t correlation_id = gpu_correlation_id();

          redshow_memory_unregister(correlation_id, md->address, md->address + md->size);

          PRINT("Free memory address %p, size %zu, op %lu\n", (void *)md->address, md->size, correlation_id);

          spinlock_unlock(&sanitizer_free_lock);
          break;
        }
      default:
        {
          break;
        }
    }
  } else if (domain == SANITIZER_CB_DOMAIN_LAUNCH) {
    Sanitizer_LaunchData *ld = (Sanitizer_LaunchData *)cbdata;
    sanitizer_thread_context = ld->context;

    static __thread dim3 grid_size = { .x = 0, .y = 0, .z = 0};
    static __thread dim3 block_size = { .x = 0, .y = 0, .z = 0};
    static __thread Sanitizer_StreamHandle priority_stream = NULL;
    static __thread bool kernel_sampling = true;
    static __thread uint64_t correlation_id = 0;
    static __thread cct_node_t *api_node = NULL;
    static __thread int32_t persistent_id = 0;

    if (cbid == SANITIZER_CBID_LAUNCH_BEGIN) {
      // Use function list to filter functions
      if (sanitizer_whitelist != NULL) {
        if (sanitizer_function_list_lookup(sanitizer_whitelist, ld->functionName) == NULL) {
          kernel_sampling = false;
        }
      }

      if (sanitizer_blacklist != NULL) {
        if (sanitizer_function_list_lookup(sanitizer_blacklist, ld->functionName) != NULL) {
          kernel_sampling = false;
        }
      }

      // Get a place holder cct node
      correlation_id = gpu_correlation_id();
      // TODO(Keren): why two extra layers?
      api_node = sanitizer_correlation_callback(correlation_id, 0);

      // Insert a function cct node
      hpcrun_safe_enter();

      gpu_op_ccts_t gpu_op_ccts;
      gpu_op_placeholder_flags_t gpu_op_placeholder_flags = 0;
      gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags, 
        gpu_placeholder_type_kernel);
      gpu_op_ccts_insert(api_node, &gpu_op_ccts, gpu_op_placeholder_flags);
      api_node = gpu_op_ccts_get(&gpu_op_ccts, gpu_placeholder_type_kernel);

      hpcrun_safe_exit();

      // Look up persisitent id
      persistent_id = hpcrun_cct_persistent_id(api_node);
      
      if (kernel_sampling) {
        // Kernel is not ignored
        // By default ignored this kernel
        kernel_sampling = false;
        int kernel_sampling_frequency = sanitizer_kernel_sampling_frequency_get();
        // TODO(Keren): thread safe rand
        kernel_sampling = rand() % kernel_sampling_frequency == 0;

        // First time must be sampled
        if (sanitizer_op_map_lookup(persistent_id) == NULL) {
          kernel_sampling = true;
          sanitizer_op_map_init(persistent_id, api_node);
        }
      }

      grid_size.x = ld->gridDim_x;
      grid_size.y = ld->gridDim_y;
      grid_size.z = ld->gridDim_z;
      block_size.x = ld->blockDim_x;
      block_size.y = ld->blockDim_y;
      block_size.z = ld->blockDim_z;

      PRINT("Launch kernel %s <%d, %d, %d>:<%d, %d, %d>, op %lu, id %d, mod_id %u\n", ld->functionName,
        ld->gridDim_x, ld->gridDim_y, ld->gridDim_z, ld->blockDim_x, ld->blockDim_y, ld->blockDim_z,
        correlation_id, persistent_id, ((hpctoolkit_cumod_st_t *)ld->module)->mod_id);

      // thread-safe
      // Create a high priority stream for the context at the first time
      // TODO(Keren): change stream->hstream
      sanitizer_context_map_stream_lock(ld->context, ld->stream);
      
      redshow_kernel_begin(sanitizer_thread_id_local, persistent_id, correlation_id);

      sanitizer_kernel_launch_callback(ld->context, ld->hStream, ld->function, grid_size, block_size, kernel_sampling);
    } else if (cbid == SANITIZER_CBID_LAUNCH_END) {
      if (kernel_sampling) {
        priority_stream = sanitizer_priority_stream_get(ld->context);

        sanitizer_kernel_launch_sync(api_node, correlation_id,
          ld->context, ld->hStream, ld->module, ld->function,
          priority_stream, grid_size, block_size);
      }

      redshow_kernel_end(sanitizer_thread_id_local, persistent_id, correlation_id);

      sanitizer_context_map_stream_unlock(ld->context, ld->stream);

      kernel_sampling = false;
    }
  } else if (domain == SANITIZER_CB_DOMAIN_MEMCPY) {
    Sanitizer_MemcpyData *md = (Sanitizer_MemcpyData *)cbdata;
    sanitizer_thread_context = md->srcContext;

    sanitizer_context_map_stream_lock(md->srcContext, md->stream);

    uint64_t correlation_id = gpu_correlation_id();
    cct_node_t *api_node = sanitizer_correlation_callback(correlation_id, 0);

    int32_t persistent_id = hpcrun_cct_persistent_id(api_node);
    if (sanitizer_op_map_lookup(persistent_id) == NULL) {
      sanitizer_op_map_init(persistent_id, api_node);
    }

    PRINT("Memcpy async %d from %p to %p, op %llu, id %d\n", md->isAsync, md->srcAddress, md->dstAddress,
      correlation_id, persistent_id);

    bool src_host = false;
    bool dst_host = false;

    if (md->direction == SANITIZER_MEMCPY_DIRECTION_HOST_TO_DEVICE) {
      src_host = true;
    } else if (md->direction == SANITIZER_MEMCPY_DIRECTION_HOST_TO_HOST) {
      src_host = true;
      dst_host = true;
    } else if (md->direction == SANITIZER_MEMCPY_DIRECTION_DEVICE_TO_HOST) {
      dst_host = true;
    }

    int32_t src_mem_id = 0;
    uint64_t src_mem_op_id = 0;
    uint64_t src_mem_addr = 0;
    uint64_t src_size = 0;
    int32_t dst_mem_id = 0;
    uint64_t dst_mem_op_id = 0;
    uint64_t dst_mem_addr = 0;
    uint64_t dst_size = 0;

    if (src_host) {
      src_mem_addr = md->srcAddress;
      src_mem_id = REDSHOW_MEMORY_HOST;
      src_mem_op_id = REDSHOW_MEMORY_HOST;
    } else {
      redshow_memory_query(correlation_id, md->srcAddress, &src_mem_id, &src_mem_op_id, &src_mem_addr, &src_size);
      // src shadow memory does not need to be updated
    }

    if (dst_host) {
      dst_mem_addr = md->dstAddress;
      dst_mem_id = REDSHOW_MEMORY_HOST;
      dst_mem_op_id = REDSHOW_MEMORY_HOST;
    } else {
      redshow_memory_query(correlation_id, md->dstAddress, &dst_mem_id, &dst_mem_op_id, &dst_mem_addr, &dst_size);
    }

    if (dst_mem_addr != 0) {
      // Avoid memcpy to symbol without allocation
      redshow_memcpy_register(persistent_id, correlation_id, src_mem_op_id, src_mem_addr, src_size,
        dst_mem_op_id, dst_mem_addr, dst_size, md->size);

      // Update shadow
      if (src_host) {
        memcpy((void *)dst_mem_addr, (void *)src_mem_addr, md->size);
      } else { 
        Sanitizer_StreamHandle priority_stream = sanitizer_priority_stream_get(md->srcContext);
        HPCRUN_SANITIZER_CALL(sanitizerMemcpyDeviceToHost,
          ((void *)dst_mem_addr, (void *)md->srcAddress, md->size, priority_stream));
      }
    }

    sanitizer_context_map_stream_unlock(md->srcContext, md->stream);
  } else if (domain == SANITIZER_CB_DOMAIN_MEMSET) {
    Sanitizer_MemsetData *md = (Sanitizer_MemsetData *)cbdata;
    sanitizer_thread_context = md->context;

    sanitizer_context_map_stream_lock(md->context, md->stream);

    uint64_t correlation_id = gpu_correlation_id();
    cct_node_t *api_node = sanitizer_correlation_callback(correlation_id, 0);

    int32_t mem_id = 0;
    uint64_t mem_op_id = 0;
    uint64_t addr = 0;
    uint64_t size = 0;

    redshow_memory_query(correlation_id, md->address, &mem_id, &mem_op_id, &addr, &size);

    int32_t persistent_id = hpcrun_cct_persistent_id(api_node);
    redshow_memset_register(persistent_id, correlation_id, mem_op_id, addr, size, md->value, md->width);
    
    // Update shadow
    memset((void *)addr, md->value, md->width);

    sanitizer_context_map_stream_unlock(md->context, md->stream);
  } else if (domain == SANITIZER_CB_DOMAIN_SYNCHRONIZE) {
    // TODO(Keren): sync data
  }
}

//******************************************************************************
// interfaces
//******************************************************************************

int
sanitizer_bind()
{
#ifndef HPCRUN_STATIC_LINK
  // dynamic libraries only availabile in non-static case
  hpcrun_force_dlopen(true);
  CHK_DLOPEN(sanitizer, sanitizer_path(), RTLD_NOW | RTLD_GLOBAL);
  hpcrun_force_dlopen(false);

#define SANITIZER_BIND(fn) \
  CHK_DLSYM(sanitizer, fn);

  FORALL_SANITIZER_ROUTINES(SANITIZER_BIND)

#undef SANITIZER_BIND

  return 0;
#else
  return -1;
#endif // ! HPCRUN_STATIC_LINK
}


static void
output_dir_config(char *dir_name, char *suffix) {
  size_t used = 0;
  used += sprintf(&dir_name[used], "%s", hpcrun_files_output_directory());
  used += sprintf(&dir_name[used], "%s", suffix);
  mkdir(dir_name, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
}


void
sanitizer_redundancy_analysis_enable()
{
  redshow_analysis_enable(REDSHOW_ANALYSIS_SPATIAL_REDUNDANCY);
  redshow_analysis_enable(REDSHOW_ANALYSIS_TEMPORAL_REDUNDANCY);

  char dir_name[PATH_MAX];
  output_dir_config(dir_name, "/redundancy/");

  redshow_output_dir_config(REDSHOW_ANALYSIS_SPATIAL_REDUNDANCY, dir_name);
  redshow_output_dir_config(REDSHOW_ANALYSIS_TEMPORAL_REDUNDANCY, dir_name);
}


void
sanitizer_data_flow_analysis_enable()
{
  redshow_analysis_enable(REDSHOW_ANALYSIS_DATA_FLOW);
  // XXX(Keren): value flow analysis must be sync
  sanitizer_analysis_async = false;

  char dir_name[PATH_MAX];
  output_dir_config(dir_name, "/data_flow/");

  redshow_output_dir_config(REDSHOW_ANALYSIS_DATA_FLOW, dir_name);
}


void
sanitizer_value_pattern_analysis_enable()
{
  redshow_analysis_enable(REDSHOW_ANALYSIS_VALUE_PATTERN);

  char dir_name[PATH_MAX];
  output_dir_config(dir_name, "/value_pattern/");

  redshow_output_dir_config(REDSHOW_ANALYSIS_VALUE_PATTERN, dir_name);
}


void
sanitizer_callbacks_subscribe() 
{
  sanitizer_correlation_callback = gpu_application_thread_correlation_callback;

  redshow_log_data_callback_register(sanitizer_log_data_callback);

  redshow_record_data_callback_register(sanitizer_record_data_callback, sanitizer_pc_views, sanitizer_mem_views);

  redshow_tool_dtoh_register(sanitizer_dtoh);

  HPCRUN_SANITIZER_CALL(sanitizerSubscribe,
    (&sanitizer_subscriber_handle, sanitizer_subscribe_callback, NULL));

  HPCRUN_SANITIZER_CALL(sanitizerEnableDomain,
    (1, sanitizer_subscriber_handle, SANITIZER_CB_DOMAIN_RESOURCE));

  HPCRUN_SANITIZER_CALL(sanitizerEnableDomain,
    (1, sanitizer_subscriber_handle, SANITIZER_CB_DOMAIN_LAUNCH));

  HPCRUN_SANITIZER_CALL(sanitizerEnableDomain,
    (1, sanitizer_subscriber_handle, SANITIZER_CB_DOMAIN_MEMCPY));

  HPCRUN_SANITIZER_CALL(sanitizerEnableDomain,
    (1, sanitizer_subscriber_handle, SANITIZER_CB_DOMAIN_MEMSET));
}


void
sanitizer_callbacks_unsubscribe() 
{
  sanitizer_correlation_callback = 0;

  HPCRUN_SANITIZER_CALL(sanitizerUnsubscribe, (sanitizer_subscriber_handle));

  HPCRUN_SANITIZER_CALL(sanitizerEnableDomain,
    (0, sanitizer_subscriber_handle, SANITIZER_CB_DOMAIN_RESOURCE));

  HPCRUN_SANITIZER_CALL(sanitizerEnableDomain,
    (0, sanitizer_subscriber_handle, SANITIZER_CB_DOMAIN_LAUNCH));

  HPCRUN_SANITIZER_CALL(sanitizerEnableDomain,
    (0, sanitizer_subscriber_handle, SANITIZER_CB_DOMAIN_MEMCPY));

  HPCRUN_SANITIZER_CALL(sanitizerEnableDomain,
    (0, sanitizer_subscriber_handle, SANITIZER_CB_DOMAIN_MEMSET));
}


void
sanitizer_async_config
(
 bool async
)
{
  sanitizer_analysis_async = async;
}


static void
function_list_add
(
 sanitizer_function_list_entry_t *head,
 char *file_name
)
{
  FILE *fp = NULL;
  fp = fopen(file_name, "r");
  if (fp != NULL) {
    char function[FUNCTION_NAME_LENGTH];
    while (fgets(function, FUNCTION_NAME_LENGTH, fp) != NULL) {
      char *pos = NULL;
      if ((pos=strchr(function, '\n')) != NULL)
        *pos = '\0';
      PRINT("Add function %s from %s\n", function, file_name);
      sanitizer_function_list_register(head, function);
    }
    fclose(fp);
  }
}


void
sanitizer_function_config
(
 char *file_whitelist,
 char *file_blacklist
)
{
  if (file_whitelist != NULL) {
    sanitizer_function_list_init(&sanitizer_whitelist);
    function_list_add(sanitizer_whitelist, file_whitelist);
  }
  if (file_blacklist != NULL) {
    sanitizer_function_list_init(&sanitizer_blacklist);
    function_list_add(sanitizer_blacklist, file_blacklist);
  }
}


void
sanitizer_buffer_config
(
 int gpu_patch_record_num,
 int buffer_pool_size
)
{
  sanitizer_gpu_patch_record_num = gpu_patch_record_num;
  sanitizer_buffer_pool_size = buffer_pool_size;
}


void
sanitizer_approx_level_config
(
 int approx_level
)
{
  sanitizer_approx_level = approx_level;

  redshow_approx_level_config(sanitizer_approx_level);
}


void
sanitizer_views_config
(
 int pc_views,
 int mem_views
)
{
  sanitizer_pc_views = pc_views;
  sanitizer_mem_views = mem_views;
}


void
sanitizer_data_type_config
(
 char *data_type
)
{
  if (data_type == NULL) {
    sanitizer_data_type = REDSHOW_DATA_UNKNOWN;
  } else if (strcmp(data_type, "float") == 0 || strcmp(data_type, "FLOAT") == 0) {
    sanitizer_data_type = REDSHOW_DATA_FLOAT;
  } else if (strcmp(data_type, "int") == 0 || strcmp(data_type, "INT") == 0) {
    sanitizer_data_type = REDSHOW_DATA_INT;
  } else {
    sanitizer_data_type = REDSHOW_DATA_UNKNOWN;
  }

  PRINT("Config data type %u\n", sanitizer_data_type);

  redshow_data_type_config(sanitizer_data_type);
}


int
sanitizer_buffer_pool_size_get()
{
  return sanitizer_buffer_pool_size;
}


void
sanitizer_stop_flag_set()
{
  sanitizer_stop_flag = true;
}


void
sanitizer_stop_flag_unset()
{
  sanitizer_stop_flag = false;
}


void
sanitizer_device_flush(void *args)
{
  if (sanitizer_stop_flag) {
    sanitizer_stop_flag_unset();

    if (sanitizer_analysis_async) {
      // Spin wait
      sanitizer_buffer_channel_flush();
      sanitizer_process_signal(); 
      while (sanitizer_buffer_channel_finish() == false) {}
    }

    // Attribute performance metrics to CCTs
    redshow_flush_thread(sanitizer_thread_id_local);
  }
}


void
sanitizer_device_shutdown(void *args)
{
  sanitizer_callbacks_unsubscribe();

  if (sanitizer_analysis_async) {
    atomic_store(&sanitizer_process_stop_flag, true);

    // Spin wait
    sanitizer_buffer_channel_flush();
    sanitizer_process_signal(); 
    while (sanitizer_buffer_channel_finish() == false) {}
  }

  // Attribute performance metrics to CCTs
  redshow_flush();

  while (atomic_load(&sanitizer_process_thread_counter));
}


void
sanitizer_process_init
(
)
{
  if (sanitizer_analysis_async) {
    pthread_t *thread = &(sanitizer_thread.thread);
    pthread_mutex_t *mutex = &(sanitizer_thread.mutex);
    pthread_cond_t *cond = &(sanitizer_thread.cond);

    // Create a new thread for the context without libmonitor watching
    monitor_disable_new_threads();

    atomic_fetch_add(&sanitizer_process_thread_counter, 1);

    pthread_mutex_init(mutex, NULL);
    pthread_cond_init(cond, NULL);
    pthread_create(thread, NULL, sanitizer_process_thread, NULL);

    monitor_enable_new_threads();
  }
}
