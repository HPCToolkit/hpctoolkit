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
  } \
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
  macro(sanitizerGetStreamHandle)          \
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
static int sanitizer_gpu_patch_addr_record_num = 0;
static int sanitizer_buffer_pool_size = 0;
static int sanitizer_pc_views = 0;
static int sanitizer_mem_views = 0;

static redshow_approx_level_t sanitizer_approx_level = REDSHOW_APPROX_NONE;
static redshow_data_type_t sanitizer_data_type = REDSHOW_DATA_FLOAT;

static size_t sanitizer_record_size = 0;
static bool sanitizer_analysis_async = false;
static uint32_t sanitizer_gpu_analysis_blocks = 0;

static __thread bool sanitizer_stop_flag = false;
static __thread uint32_t sanitizer_thread_id_self = (1 << 30);
static __thread uint32_t sanitizer_thread_id_local = 0;
static __thread CUcontext sanitizer_thread_context = NULL;

static __thread gpu_patch_buffer_t gpu_patch_buffer_reset = {
  .full = 0,
  .analysis = 0,
  .head_index = 0,
  .tail_index = 0,
  .size = 0,
  .type = GPU_PATCH_DEFAULT,
  .flags = GPU_PATCH_NONE
};

static __thread gpu_patch_buffer_t gpu_patch_buffer_addr_read_reset = {
  .full = 0,
  .analysis = 1,
  .head_index = 0,
  .tail_index = 0,
  .size = 0,
  .type = GPU_PATCH_ADDRESS_ANALYSIS,
  .flags = GPU_PATCH_READ | GPU_PATCH_ANALYSIS
};

static __thread gpu_patch_buffer_t gpu_patch_buffer_addr_write_reset = {
  .full = 0,
  .analysis = 1,
  .head_index = 0,
  .tail_index = 0,
  .size = 0,
  .type = GPU_PATCH_ADDRESS_ANALYSIS,
  .flags = GPU_PATCH_WRITE | GPU_PATCH_ANALYSIS
};

// Host buffers are per-thread
static __thread gpu_patch_buffer_t *gpu_patch_buffer_host = NULL;
static __thread gpu_patch_buffer_t *gpu_patch_buffer_addr_read_host = NULL;
static __thread gpu_patch_buffer_t *gpu_patch_buffer_addr_write_host = NULL;
static __thread gpu_patch_buffer_t *gpu_patch_buffer_addr_read_host_records = NULL;
static __thread gpu_patch_buffer_t *gpu_patch_buffer_addr_write_host_records = NULL;

// Device buffers are per-context
static __thread gpu_patch_buffer_t *gpu_patch_buffer_device = NULL;
static __thread gpu_patch_buffer_t *gpu_patch_buffer_addr_read_device = NULL;
static __thread gpu_patch_buffer_t *gpu_patch_buffer_addr_write_device = NULL;

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


SANITIZER_FN
(
 sanitizerGetStreamHandle,
 (
  CUcontext ctx,
  CUstream stream,
  Sanitizer_StreamHandle *hStream
 )
);

//******************************************************************************
// forward declaration operations
//******************************************************************************

static Sanitizer_StreamHandle sanitizer_priority_stream_get(CUcontext context);

static Sanitizer_StreamHandle sanitizer_kernel_stream_get(CUcontext context);

static void sanitizer_kernel_launch(CUcontext context);

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
    strcat(buffer, "compute-sanitizer/libsanitizer-public.so");
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
sanitizer_buffer_init
(
 CUcontext context
)
{
  // Get cached entry
  sanitizer_context_map_entry_t *entry = sanitizer_context_map_init(context);
  Sanitizer_StreamHandle priority_stream = sanitizer_priority_stream_get(context);

  gpu_patch_buffer_device = sanitizer_context_map_entry_buffer_device_get(entry);
  gpu_patch_buffer_addr_read_device = sanitizer_context_map_entry_buffer_addr_read_device_get(entry);
  gpu_patch_buffer_addr_write_device = sanitizer_context_map_entry_buffer_addr_write_device_get(entry);

  if (gpu_patch_buffer_device == NULL) {
    // Allocate buffer
    void *gpu_patch_records = NULL;

    // gpu_patch_buffer
    HPCRUN_SANITIZER_CALL(sanitizerAlloc, (context, (void **)(&(gpu_patch_buffer_device)), sizeof(gpu_patch_buffer_t)));
    HPCRUN_SANITIZER_CALL(sanitizerMemset, (gpu_patch_buffer_device, 0, sizeof(gpu_patch_buffer_t), priority_stream));

    PRINT("Allocate gpu_patch_buffer %p, size %zu\n", gpu_patch_buffer_device, sizeof(gpu_patch_buffer_t));

    // gpu_patch_buffer_t->records
    HPCRUN_SANITIZER_CALL(sanitizerAlloc,
      (context, &gpu_patch_records, sanitizer_gpu_patch_record_num * sanitizer_record_size));
    HPCRUN_SANITIZER_CALL(sanitizerMemset,
      (gpu_patch_records, 0, sanitizer_gpu_patch_record_num * sanitizer_record_size, priority_stream));
    PRINT("Allocate gpu_patch_records %p, size %zu\n", gpu_patch_records, sanitizer_gpu_patch_record_num * sanitizer_record_size);

    gpu_patch_buffer_reset.records = gpu_patch_records;
    gpu_patch_buffer_reset.size = sanitizer_gpu_patch_record_num;

    HPCRUN_SANITIZER_CALL(sanitizerMemcpyHostToDeviceAsync,
      (gpu_patch_buffer_device, &gpu_patch_buffer_reset, sizeof(gpu_patch_buffer_t), priority_stream));

    // Update map
    sanitizer_context_map_buffer_device_update(context, gpu_patch_buffer_device);

    if (sanitizer_gpu_analysis_blocks != 0) {
      // Read
      HPCRUN_SANITIZER_CALL(sanitizerAlloc, (context, (void **)(&(gpu_patch_buffer_addr_read_device)), sizeof(gpu_patch_buffer_t)));
      HPCRUN_SANITIZER_CALL(sanitizerMemset, (gpu_patch_buffer_addr_read_device, 0, sizeof(gpu_patch_buffer_t), priority_stream));
      PRINT("Allocate gpu_patch_buffer_addr_read_device %p, size %zu\n", gpu_patch_buffer_addr_read_device, sizeof(gpu_patch_buffer_t));

      // gpu_patch_buffer_t->records
      HPCRUN_SANITIZER_CALL(sanitizerAlloc,
        (context, &gpu_patch_records, sanitizer_gpu_patch_addr_record_num * sizeof(gpu_patch_analysis_address_t)));
      HPCRUN_SANITIZER_CALL(sanitizerMemset,
        (gpu_patch_records, 0, sanitizer_gpu_patch_addr_record_num * sizeof(gpu_patch_analysis_address_t), priority_stream));
      PRINT("Allocate gpu_patch_records %p, size %zu\n", gpu_patch_records, sanitizer_gpu_patch_addr_record_num * sizeof(gpu_patch_analysis_address_t));

      gpu_patch_buffer_addr_read_reset.records = gpu_patch_records;
      gpu_patch_buffer_addr_read_reset.size = sanitizer_gpu_patch_addr_record_num;

      HPCRUN_SANITIZER_CALL(sanitizerMemcpyHostToDeviceAsync,
        (gpu_patch_buffer_addr_read_device, &gpu_patch_buffer_addr_read_reset, sizeof(gpu_patch_buffer_t), priority_stream));

      // Write
      HPCRUN_SANITIZER_CALL(sanitizerAlloc, (context, (void **)(&(gpu_patch_buffer_addr_write_device)), sizeof(gpu_patch_buffer_t)));
      HPCRUN_SANITIZER_CALL(sanitizerMemset, (gpu_patch_buffer_addr_write_device, 0, sizeof(gpu_patch_buffer_t), priority_stream));
      PRINT("Allocate gpu_patch_buffer_addr_write_device %p, size %zu\n", gpu_patch_buffer_addr_write_device, sizeof(gpu_patch_buffer_t));

      // gpu_patch_buffer_t->records
      HPCRUN_SANITIZER_CALL(sanitizerAlloc,
        (context, &gpu_patch_records, sanitizer_gpu_patch_addr_record_num * sizeof(gpu_patch_analysis_address_t)));
      HPCRUN_SANITIZER_CALL(sanitizerMemset,
        (gpu_patch_records, 0, sanitizer_gpu_patch_addr_record_num * sizeof(gpu_patch_analysis_address_t), priority_stream));
      PRINT("Allocate gpu_patch_records %p, size %zu\n", gpu_patch_records, sanitizer_gpu_patch_addr_record_num * sizeof(gpu_patch_analysis_address_t));

      gpu_patch_buffer_addr_write_reset.records = gpu_patch_records;
      gpu_patch_buffer_addr_write_reset.size = sanitizer_gpu_patch_addr_record_num;

      HPCRUN_SANITIZER_CALL(sanitizerMemcpyHostToDeviceAsync,
        (gpu_patch_buffer_addr_write_device, &gpu_patch_buffer_addr_write_reset, sizeof(gpu_patch_buffer_t), priority_stream));

      // Update map
      sanitizer_context_map_buffer_addr_read_device_update(context, gpu_patch_buffer_addr_read_device);
      sanitizer_context_map_buffer_addr_write_device_update(context, gpu_patch_buffer_addr_write_device);
    }

    // Ensure data copy is done
    HPCRUN_SANITIZER_CALL(sanitizerStreamSynchronize, (priority_stream));
  }
}


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

  PRINT("Context %p Patch CUBIN: \n", context);
  PRINT("%s\n", HPCTOOLKIT_GPU_PATCH);
  // patch binary
  if (gpu_patch_buffer_reset.type == GPU_PATCH_ADDRESS_PATCH) {
    // Only analyze global memory
    HPCRUN_SANITIZER_CALL(sanitizerAddPatchesFromFile, (HPCTOOLKIT_GPU_PATCH "gpu-patch-address.fatbin", context));
    HPCRUN_SANITIZER_CALL(sanitizerPatchInstructions,
      (SANITIZER_INSTRUCTION_GLOBAL_MEMORY_ACCESS, module, "sanitizer_global_memory_access_callback"));
  } else {
    HPCRUN_SANITIZER_CALL(sanitizerAddPatchesFromFile, (HPCTOOLKIT_GPU_PATCH "gpu-patch.fatbin", context));
    HPCRUN_SANITIZER_CALL(sanitizerPatchInstructions,
      (SANITIZER_INSTRUCTION_GLOBAL_MEMORY_ACCESS, module, "sanitizer_global_memory_access_callback"));
    HPCRUN_SANITIZER_CALL(sanitizerPatchInstructions,
      (SANITIZER_INSTRUCTION_SHARED_MEMORY_ACCESS, module, "sanitizer_shared_memory_access_callback"));
    HPCRUN_SANITIZER_CALL(sanitizerPatchInstructions,
      (SANITIZER_INSTRUCTION_LOCAL_MEMORY_ACCESS, module, "sanitizer_local_memory_access_callback"));
    HPCRUN_SANITIZER_CALL(sanitizerPatchInstructions,
      (SANITIZER_INSTRUCTION_BLOCK_ENTER, module, "sanitizer_block_enter_callback"));
  }
  HPCRUN_SANITIZER_CALL(sanitizerPatchInstructions,
    (SANITIZER_INSTRUCTION_BLOCK_EXIT, module, "sanitizer_block_exit_callback"));
  HPCRUN_SANITIZER_CALL(sanitizerPatchModule, (module));

  sanitizer_buffer_init(context);
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

  Sanitizer_StreamHandle priority_stream_handle =
    sanitizer_context_map_entry_priority_stream_handle_get(entry);

  if (priority_stream_handle == NULL) {
    // First time
    // Update priority stream
    CUstream priority_stream = sanitizer_context_map_entry_priority_stream_get(entry);
    HPCRUN_SANITIZER_CALL(sanitizerGetStreamHandle, (context, priority_stream, &priority_stream_handle)); 
    sanitizer_context_map_priority_stream_handle_update(context, priority_stream_handle);
  }

  return priority_stream_handle;
}


static Sanitizer_StreamHandle
sanitizer_kernel_stream_get
(
 CUcontext context
)
{
  sanitizer_context_map_entry_t *entry = sanitizer_context_map_init(context);

  Sanitizer_StreamHandle kernel_stream_handle =
    sanitizer_context_map_entry_kernel_stream_handle_get(entry);

  if (kernel_stream_handle == NULL) {
    // First time
    // Update kernel stream
    CUstream kernel_stream = sanitizer_context_map_entry_kernel_stream_get(entry);
    HPCRUN_SANITIZER_CALL(sanitizerGetStreamHandle, (context, kernel_stream, &kernel_stream_handle)); 
    sanitizer_context_map_kernel_stream_handle_update(context, kernel_stream_handle);
  }

  return kernel_stream_handle;
}


static void
sanitizer_kernel_launch
(
 CUcontext context
)
{
  sanitizer_context_map_entry_t *entry = sanitizer_context_map_init(context);

  // Get raw priority stream and function
  CUfunction analysis_function = sanitizer_context_map_entry_analysis_function_get(entry);
  CUstream kernel_stream = sanitizer_context_map_entry_kernel_stream_get(entry);

  // First time get function
  if (analysis_function == NULL) {
    CUmodule analysis_module = NULL;
    cuda_module_load(&analysis_module, HPCTOOLKIT_GPU_PATCH "gpu-analysis.fatbin");
    cuda_module_function_get(&analysis_function, analysis_module, "gpu_analysis_interval_merge");

    sanitizer_context_map_analysis_function_update(context, analysis_function);

    PRINT("Sanitizer context %p load function gpu_analysis_interval_merge %p\n", context, analysis_function);
  }

  // Launch analysis function
  if (analysis_function != NULL) {
    gpu_patch_buffer_device = sanitizer_context_map_entry_buffer_device_get(entry);
    gpu_patch_buffer_addr_read_device = sanitizer_context_map_entry_buffer_addr_read_device_get(entry);
    gpu_patch_buffer_addr_write_device = sanitizer_context_map_entry_buffer_addr_write_device_get(entry);

    void *args[] = { (void *)&gpu_patch_buffer_device, (void *)&gpu_patch_buffer_addr_read_device,
      (void *)&gpu_patch_buffer_addr_write_device };

    cuda_kernel_launch(analysis_function, sanitizer_gpu_analysis_blocks, 1, 1,
      GPU_PATCH_ANALYSIS_THREADS, 1, 1, 0, kernel_stream, args);

    PRINT("Sanitizer context %p launch function %p <%u, %u>\n", \
      context, analysis_function, sanitizer_gpu_analysis_blocks, GPU_PATCH_ANALYSIS_THREADS);
  }
}


static void
sanitizer_kernel_analyze
(
 cct_node_t *api_node,
 uint64_t correlation_id,
 uint32_t cubin_id,
 uint32_t mod_id,
 Sanitizer_StreamHandle priority_stream,
 Sanitizer_StreamHandle kernel_stream,
 bool analysis_end
)
{
  HPCRUN_SANITIZER_CALL(sanitizerMemcpyDeviceToHost,
    (gpu_patch_buffer_addr_read_host, gpu_patch_buffer_addr_read_device,
     sizeof(gpu_patch_buffer_t), priority_stream));

  HPCRUN_SANITIZER_CALL(sanitizerMemcpyDeviceToHost,
    (gpu_patch_buffer_addr_write_host, gpu_patch_buffer_addr_write_device,
     sizeof(gpu_patch_buffer_t), priority_stream));

  // Data is analyzed on the GPU side, directly pass it to redshow
  size_t num_records_read = gpu_patch_buffer_addr_read_host->head_index;
  size_t num_records_write = gpu_patch_buffer_addr_write_host->head_index;
  int32_t persistent_id = hpcrun_cct_persistent_id(api_node);

  if (analysis_end) {
    // Last analysis
    // Write
    void *gpu_patch_record_device = gpu_patch_buffer_addr_write_host->records;
    HPCRUN_SANITIZER_CALL(sanitizerMemcpyDeviceToHost,
      (gpu_patch_buffer_addr_write_host_records, gpu_patch_record_device,
       sizeof(gpu_patch_analysis_address_t) * num_records_write, priority_stream));

    // Read
    gpu_patch_record_device = gpu_patch_buffer_addr_read_host->records;
    HPCRUN_SANITIZER_CALL(sanitizerMemcpyDeviceToHost,
      (gpu_patch_buffer_addr_read_host_records, gpu_patch_record_device,
       sizeof(gpu_patch_analysis_address_t) * num_records_read, priority_stream));

    // Tell analysis kernel is done
    gpu_patch_buffer_addr_read_host->full = 0;
    gpu_patch_buffer_addr_read_host->analysis = 0;
    gpu_patch_buffer_addr_write_host->full = 0;
    gpu_patch_buffer_addr_write_host->analysis = 0;

    HPCRUN_SANITIZER_CALL(sanitizerMemcpyHostToDeviceAsync,
      (gpu_patch_buffer_addr_read_device, gpu_patch_buffer_addr_read_host,
       offsetof(gpu_patch_buffer_t, head_index), priority_stream));
    HPCRUN_SANITIZER_CALL(sanitizerMemcpyHostToDeviceAsync,
      (gpu_patch_buffer_addr_write_device, gpu_patch_buffer_addr_write_host,
       offsetof(gpu_patch_buffer_t, head_index), priority_stream));
    
    gpu_patch_buffer_addr_write_host->records = gpu_patch_buffer_addr_write_host_records;
    redshow_analyze(sanitizer_thread_id_local, cubin_id, mod_id, persistent_id, correlation_id,
      gpu_patch_buffer_addr_write_host);

    gpu_patch_buffer_addr_read_host->records = gpu_patch_buffer_addr_read_host_records;
    redshow_analyze(sanitizer_thread_id_local, cubin_id, mod_id, persistent_id, correlation_id,
      gpu_patch_buffer_addr_read_host);

    // Do not enter later ode
    PRINT("Sanitizer analysis kernel done\n");
    return;
  }

  if (gpu_patch_buffer_addr_write_host->full != 0) {
    void *gpu_patch_record_device = gpu_patch_buffer_addr_write_host->records;
    HPCRUN_SANITIZER_CALL(sanitizerMemcpyDeviceToHost,
      (gpu_patch_buffer_addr_write_host_records, gpu_patch_record_device,
       sizeof(gpu_patch_analysis_address_t) * num_records_write, priority_stream));

    // Tell analysis kernel to continue
    gpu_patch_buffer_addr_write_host->full = 0;
    HPCRUN_SANITIZER_CALL(sanitizerMemcpyHostToDeviceAsync,
      (gpu_patch_buffer_addr_write_device, gpu_patch_buffer_addr_write_host,
       sizeof(gpu_patch_buffer_host->full), priority_stream));

    gpu_patch_buffer_addr_write_host->records = gpu_patch_buffer_addr_write_host_records;
    redshow_analyze(sanitizer_thread_id_local, cubin_id, mod_id, persistent_id, correlation_id,
      gpu_patch_buffer_addr_write_host);

    PRINT("Sanitizer analysis kernel analysis in process\n");
  }

  // Copy all records to the cache
  if (gpu_patch_buffer_addr_read_host->full != 0) {
    void *gpu_patch_record_device = gpu_patch_buffer_addr_read_host->records;
    HPCRUN_SANITIZER_CALL(sanitizerMemcpyDeviceToHost,
      (gpu_patch_buffer_addr_read_host_records, gpu_patch_record_device,
       sizeof(gpu_patch_analysis_address_t) * num_records_read, priority_stream));

    // Tell analysis kernel to continue
    gpu_patch_buffer_addr_read_host->full = 0;
    // Tell analysis kernel to continue: only in read 
    HPCRUN_SANITIZER_CALL(sanitizerMemcpyHostToDeviceAsync,
      (gpu_patch_buffer_addr_read_device, gpu_patch_buffer_addr_read_host,
       sizeof(gpu_patch_buffer_host->full), priority_stream));

    gpu_patch_buffer_addr_read_host->records = gpu_patch_buffer_addr_read_host_records;
    redshow_analyze(sanitizer_thread_id_local, cubin_id, mod_id, persistent_id, correlation_id,
      gpu_patch_buffer_addr_read_host);

    PRINT("Sanitizer analysis kernel analysis in process\n");
  }
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
 Sanitizer_StreamHandle kernel_stream,
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

    if (sanitizer_gpu_analysis_blocks != 0) {
      gpu_patch_buffer_addr_read_host = (gpu_patch_buffer_t *)hpcrun_malloc_safe(sizeof(gpu_patch_buffer_t));
      gpu_patch_buffer_addr_write_host = (gpu_patch_buffer_t *)hpcrun_malloc_safe(sizeof(gpu_patch_buffer_t));
      gpu_patch_buffer_addr_read_host_records = hpcrun_malloc_safe(
        sizeof(gpu_patch_analysis_address_t) * sanitizer_gpu_patch_addr_record_num);
      gpu_patch_buffer_addr_write_host_records = hpcrun_malloc_safe(
        sizeof(gpu_patch_analysis_address_t) * sanitizer_gpu_patch_addr_record_num);

      PRINT("Sanitizer allocate records size %zu\n", sizeof(gpu_patch_analysis_address_t) * sanitizer_gpu_patch_addr_record_num);
    }
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

    if (sanitizer_gpu_analysis_blocks != 0) {
      sanitizer_kernel_analyze(api_node, correlation_id, cubin_id, mod_id, priority_stream, kernel_stream, false);
    }

    // Wait until the buffer is full or the kernel is finished
    if (!(gpu_patch_buffer_host->num_threads == num_left_threads || gpu_patch_buffer_host->full) || num_records == 0) {
      continue;
    }

    // Reserve for debugging correctness
    //PRINT("num_records %zu\n", num_records);

    if (sanitizer_gpu_analysis_blocks == 0) {
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
      void *gpu_patch_record_device = gpu_patch_buffer_host->records;
      HPCRUN_SANITIZER_CALL(sanitizerMemcpyDeviceToHost,
        (gpu_patch_buffer->records, gpu_patch_record_device, sanitizer_record_size * num_records, priority_stream));

      // Tell gpu copy is finished
      gpu_patch_buffer_host->full = 0;
      // Do not need to sync stream.
      // The function will return once the pageable buffer has been copied to the staging memory.
      // for DMA transfer to device memory, but the DMA to final destination may not have completed.
      // Only copy the first field because other fields are being updated by the GPU.
      HPCRUN_SANITIZER_CALL(sanitizerMemcpyHostToDeviceAsync,
        (gpu_patch_buffer_device, gpu_patch_buffer_host, sizeof(gpu_patch_buffer_host->full), priority_stream));
    
      sanitizer_buffer_channel_push(sanitizer_buffer);

      PRINT("Sanitizer analysis cpu in process\n");
    }

    // Awake background thread
    if (sanitizer_analysis_async) {
      // If multiple application threads are created, it might miss a signal,
      // but we finally still process all the records
      sanitizer_process_signal(); 
    }
    
    // Finish all the threads
    if (gpu_patch_buffer_host->num_threads == num_left_threads) {
      break;
    }
  }

  if (sanitizer_gpu_analysis_blocks != 0) {
    // Kernel is done
    sanitizer_kernel_analyze(api_node, correlation_id, cubin_id, mod_id, priority_stream, kernel_stream, true);
  }

  // To ensure previous kernel is done
  HPCRUN_SANITIZER_CALL(sanitizerStreamSynchronize, (handle_stream));

  HPCRUN_SANITIZER_CALL(sanitizerStreamSynchronize, (priority_stream));

  if (sanitizer_gpu_analysis_blocks != 0) {
    HPCRUN_SANITIZER_CALL(sanitizerStreamSynchronize, (kernel_stream));
  }

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

  // Get cached entry
  if (gpu_patch_buffer_device == NULL) {
    sanitizer_context_map_entry_t *entry = sanitizer_context_map_lookup(context);

    gpu_patch_buffer_device = sanitizer_context_map_entry_buffer_device_get(entry);
    gpu_patch_buffer_addr_read_device = sanitizer_context_map_entry_buffer_addr_read_device_get(entry);
    gpu_patch_buffer_addr_write_device = sanitizer_context_map_entry_buffer_addr_write_device_get(entry);
  }

  // reset buffer
  gpu_patch_buffer_reset.num_threads = grid_dim * block_dim;
  gpu_patch_buffer_reset.block_sampling_frequency = block_sampling_frequency;
  gpu_patch_buffer_reset.block_sampling_offset = block_sampling_offset;

  HPCRUN_SANITIZER_CALL(sanitizerMemcpyHostToDeviceAsync,
    (gpu_patch_buffer_device, &gpu_patch_buffer_reset,
     sizeof(gpu_patch_buffer_t), handle_stream));

  if (sanitizer_gpu_analysis_blocks != 0) {
    HPCRUN_SANITIZER_CALL(sanitizerMemcpyHostToDeviceAsync,
      (gpu_patch_buffer_addr_read_device, &gpu_patch_buffer_addr_read_reset,
       sizeof(gpu_patch_buffer_t), handle_stream));
    HPCRUN_SANITIZER_CALL(sanitizerMemcpyHostToDeviceAsync,
      (gpu_patch_buffer_addr_write_device, &gpu_patch_buffer_addr_write_reset,
       sizeof(gpu_patch_buffer_t), handle_stream));
  }

  HPCRUN_SANITIZER_CALL(sanitizerSetCallbackData, (function, gpu_patch_buffer_device));
  
  // To ensure previous copies are done
  HPCRUN_SANITIZER_CALL(sanitizerStreamSynchronize, (handle_stream));

  if (sanitizer_gpu_analysis_blocks != 0) {
    sanitizer_kernel_launch(context);
  }
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
  if (cuda_api_internal()) {
    return;
  }

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
          hpcrun_cct_retain(api_node);

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
    static __thread Sanitizer_StreamHandle kernel_stream = NULL;
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
      hpcrun_cct_retain(api_node);

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
      sanitizer_context_map_context_lock(ld->context);
      
      redshow_kernel_begin(sanitizer_thread_id_local, persistent_id, correlation_id);

      sanitizer_kernel_launch_callback(ld->context, ld->hStream, ld->function, grid_size, block_size, kernel_sampling);
    } else if (cbid == SANITIZER_CBID_LAUNCH_END) {
      if (kernel_sampling) {
        PRINT("Sync kernel %s\n", ld->functionName);

        priority_stream = sanitizer_priority_stream_get(ld->context);
        kernel_stream = sanitizer_kernel_stream_get(ld->context);

        sanitizer_kernel_launch_sync(api_node, correlation_id,
          ld->context, ld->hStream, ld->module, ld->function,
          priority_stream, kernel_stream, grid_size, block_size);
      }

      redshow_kernel_end(sanitizer_thread_id_local, persistent_id, correlation_id);

      sanitizer_context_map_context_unlock(ld->context);

      kernel_sampling = true;

      PRINT("kernel %s done\n", ld->functionName);
    }
  } else if (domain == SANITIZER_CB_DOMAIN_MEMCPY) {
    Sanitizer_MemcpyData *md = (Sanitizer_MemcpyData *)cbdata;
    sanitizer_thread_context = md->srcContext;

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

    if (!src_host) {
      sanitizer_context_map_context_lock(md->srcContext);
    }

    if (!dst_host && md->hSrcStream != md->hDstStream) {
      // Use handles to differentiate NULL streams in two contexts
      sanitizer_context_map_context_lock(md->dstContext);
    }

    uint64_t correlation_id = gpu_correlation_id();
    cct_node_t *api_node = sanitizer_correlation_callback(correlation_id, 0);
    hpcrun_cct_retain(api_node);

    int32_t persistent_id = hpcrun_cct_persistent_id(api_node);
    if (sanitizer_op_map_lookup(persistent_id) == NULL) {
      sanitizer_op_map_init(persistent_id, api_node);
    }

    PRINT("Memcpy async %d direction %d from %p to %p, op %lu, id %d\n", md->isAsync, md->direction,
      (void *)md->srcAddress, (void *)md->dstAddress, correlation_id, persistent_id);

    // Avoid memcpy to symbol without allocation
    // Let redshow update shadow memory
    redshow_memcpy_register(persistent_id, correlation_id, src_host, md->srcAddress,
      dst_host, md->dstAddress, md->size);

    if (!src_host) {
      sanitizer_context_map_context_unlock(md->srcContext);
    }

    if (!dst_host && md->hSrcStream != md->hDstStream) {
      sanitizer_context_map_context_unlock(md->dstContext);
    }
  } else if (domain == SANITIZER_CB_DOMAIN_MEMSET) {
    Sanitizer_MemsetData *md = (Sanitizer_MemsetData *)cbdata;
    sanitizer_thread_context = md->context;

    sanitizer_context_map_context_lock(md->context);

    uint64_t correlation_id = gpu_correlation_id();
    cct_node_t *api_node = sanitizer_correlation_callback(correlation_id, 0);
    hpcrun_cct_retain(api_node);

    // Let redshow update shadow
    int32_t persistent_id = hpcrun_cct_persistent_id(api_node);
    redshow_memset_register(persistent_id, correlation_id, md->address, md->value, md->width);

    sanitizer_context_map_context_unlock(md->context);
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

  sanitizer_record_size = sizeof(gpu_patch_record_t);
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

  // Address only patch
  gpu_patch_buffer_reset.type = GPU_PATCH_ADDRESS_PATCH;
  if (sanitizer_gpu_analysis_blocks != 0) {
    gpu_patch_buffer_reset.flags |= GPU_PATCH_ANALYSIS;
  }
  sanitizer_record_size = sizeof(gpu_patch_record_address_t);
}


void
sanitizer_value_pattern_analysis_enable()
{
  redshow_analysis_enable(REDSHOW_ANALYSIS_VALUE_PATTERN);

  char dir_name[PATH_MAX];
  output_dir_config(dir_name, "/value_pattern/");

  redshow_output_dir_config(REDSHOW_ANALYSIS_VALUE_PATTERN, dir_name);

  sanitizer_record_size = sizeof(gpu_patch_record_t);
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
  sanitizer_gpu_patch_addr_record_num = gpu_patch_record_num * GPU_PATCH_WARP_SIZE * 2;
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


size_t
sanitizer_record_size_get()
{
  return sanitizer_record_size;
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
sanitizer_gpu_analysis_config(int gpu_analysis_blocks)
{
  sanitizer_gpu_analysis_blocks = gpu_analysis_blocks;
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
sanitizer_init
(
)
{
  sanitizer_stop_flag = false;
  sanitizer_thread_id_local = 0;
  sanitizer_thread_context = NULL;

  gpu_patch_buffer_device = NULL;
  gpu_patch_buffer_addr_read_device = NULL;
  gpu_patch_buffer_addr_write_device = NULL;

  gpu_patch_buffer_host = NULL;
  gpu_patch_buffer_addr_read_host = NULL;
  gpu_patch_buffer_addr_write_host = NULL;
  gpu_patch_buffer_addr_read_host_records = NULL;
  gpu_patch_buffer_addr_write_host_records = NULL;

  atomic_store(&sanitizer_process_awake_flag, false);
  atomic_store(&sanitizer_process_stop_flag, false);
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
