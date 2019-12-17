#include <stdio.h>
#include <errno.h>     // errno
#include <fcntl.h>     // open
#include <limits.h>    // PATH_MAX
#include <stdio.h>     // sprintf
#include <unistd.h>
#include <sys/stat.h>  // mkdir
#include <string.h>    // strstr

#ifndef HPCRUN_STATIC_LINK
#include <dlfcn.h>
#undef _GNU_SOURCE
#define _GNU_SOURCE
#include <link.h>          // dl_iterate_phdr
#include <linux/limits.h>  // PATH_MAX
#include <string.h>        // strstr
#endif

#include <sanitizer.h>

#include <lib/prof-lean/spinlock.h>
#include <lib/prof-lean/stdatomic.h>

#include <hpcrun/cct2metrics.h>
#include <hpcrun/files.h>
#include <hpcrun/hpcrun_stats.h>
#include <hpcrun/module-ignore-map.h>
#include <hpcrun/main.h>
#include <hpcrun/safe-sampling.h>
#include <hpcrun/sample_event.h>
#include <hpcrun/sample-sources/libdl.h>

#include "nvidia.h"

#include "cuda-api.h"
#include "cuda-state-placeholders.h"

#include "cubin-module-map.h"
#include "cubin-hash-map.h"

#include "sanitizer-api.h"
#include "sanitizer-record.h"
#include "sanitizer-node.h"
#include "sanitizer-stream-map.h"
#include "sanitizer-context-map.h"


#define SANITIZER_API_DEBUG 1

#if SANITIZER_API_DEBUG
#define PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define PRINT(...)
#endif

#define MIN2(m1, m2) m1 > m2 ? m2 : m1

#define GPU_PATCH_RECORD_NUM (256 * 1024)
#define GPU_PATCH_BUFFER_POOL_NUM (128)

#define CUFUNCTION_SYMBOL_INDEX_OFFSET (16)
#define CUFUNCTION_FUNCTION_RECORD_OFFSET (16*8+8)
#define FUNCTION_RECORD_FUNCTION_ADDR_OFFSET (16*3)

#define SANITIZER_FN_NAME(f) DYN_FN_NAME(f)

#define SANITIZER_FN(fn, args) \
  static SanitizerResult (*SANITIZER_FN_NAME(fn)) args

#define HPCRUN_SANITIZER_CALL(fn, args) \
{      \
  SanitizerResult status = SANITIZER_FN_NAME(fn) args;	\
  if (status != SANITIZER_SUCCESS) {		\
    sanitizer_error_report(status, #fn);		\
  }						\
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
  macro(sanitizerAddPatchesFromFile)       \
  macro(sanitizerPatchInstructions)        \
  macro(sanitizerPatchModule)              \
  macro(sanitizerGetResultString)          \
  macro(sanitizerUnpatchModule)

// only subscribe by the main thread
static spinlock_t cubin_files_lock = SPINLOCK_UNLOCKED;
static spinlock_t allocate_buffer_lock = SPINLOCK_UNLOCKED;
static Sanitizer_SubscriberHandle sanitizer_subscriber_handle;

static __thread bool sanitizer_stop_flag = false;
static __thread uint32_t sanitizer_thread_id = 0;
static atomic_uint sanitizer_global_thread_id = ATOMIC_VAR_INIT(0);
static atomic_uint sanitizer_allocated_buffer_num = ATOMIC_VAR_INIT(0);

static __thread gpu_patch_buffer_t gpu_patch_buffer_reset = {
 .head_index = 0,
 .tail_index = 0,
 .size = GPU_PATCH_RECORD_NUM,
 .full = 0
};
static __thread gpu_patch_buffer_t *gpu_patch_buffer = NULL;
static __thread gpu_patch_record_t *gpu_patch_record = NULL;
static __thread gpu_patch_buffer_t *gpu_patch_buffer_device = NULL;
static __thread char *sanitizer_trace = NULL;
static __thread dim3 grid_size;
static __thread dim3 block_size;

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
 sanitizerEnableAllDomains,
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
  CUstream stream
 )
);


SANITIZER_FN
(
 sanitizerMemcpyDeviceToHost,
 (
  void* dst,
  void* src,
  size_t count,
  CUstream stream
 ) 
);


SANITIZER_FN
(
 sanitizerMemcpyHostToDeviceAsync,
 (
  void* dst,
  void* src,
  size_t count,
  CUstream stream
 ) 
);


SANITIZER_FN
(
 sanitizerSetCallbackData,
 (
  CUstream stream,
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
 sanitizerUnpatchModule,
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


static cct_node_t *
sanitizer_correlation_callback
(
 cuda_placeholder_t cuda_state
);


//******************************************************************************
// private operations
//******************************************************************************

static void
sanitizer_error_callback // __attribute__((unused))
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

#ifndef HPCRUN_STATIC_LINK
static const char *
sanitizer_path
(
 void
)
{
  // TODO(Keren): change it back after 10.2 public release
  const char *path = "/home/kz21/Codes/Sanitizer/libsanitizer-public.so";

#if 0
  static char buffer[PATH_MAX];
  buffer[0] = 0;

  // open an NVIDIA library to find the CUDA path with dl_iterate_phdr
  // note: a version of this file with a more specific name may 
  // already be loaded. thus, even if the dlopen fails, we search with
  // dl_iterate_phdr.
  void *h = monitor_real_dlopen("libcudart.so", RTLD_LOCAL | RTLD_LAZY);

  if (dl_iterate_phdr(cuda_path, buffer)) {
    // invariant: buffer contains CUDA home 
    strcat(buffer, "extras/Sanitizer/libsanitizer-public.so");
    path = buffer;
  }

  if (h) monitor_real_dlclose(h);
#endif

  return path;
}

#endif

static bool
sanitizer_write_cubin
(
 const char *file_name,
 const void *cubin,
 size_t cubin_size
)
{
  int fd;
  errno = 0;
  fd = open(file_name, O_WRONLY | O_CREAT | O_EXCL, 0644);
  if (errno == EEXIST) {
    close(fd);
    return true;
  }
  if (fd >= 0) {
    // Success
    if (write(fd, cubin, cubin_size) != cubin_size) {
      close(fd);
      return false;   
    } else {
      close(fd);
      return true;
    }
  } else {
    // Failure to open is a fatal error.
    hpcrun_abort("hpctoolkit: unable to open file: '%s'", file_name);
    return false;
  }
}


static void
sanitizer_load_callback
(
 CUcontext context,
 CUmodule cubin_module, 
 const void *cubin, 
 size_t cubin_size
)
{
  // Compute hash for cubin and store it into a map
  cubin_hash_map_entry_t *entry = cubin_hash_map_lookup(cubin);
  unsigned char *hash;
  unsigned int hash_len;
  if (entry == NULL) {
    cubin_hash_map_insert(cubin, cubin_size);
    entry = cubin_hash_map_lookup(cubin);
  }
  hash = cubin_hash_map_entry_hash_get(entry, &hash_len);

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

  // Write a file if does not exist
  bool file_flag;
  spinlock_lock(&cubin_files_lock);
  file_flag = sanitizer_write_cubin(file_name, cubin, cubin_size);
  spinlock_unlock(&cubin_files_lock);

  if (file_flag) {
    char device_file[PATH_MAX]; 
    sprintf(device_file, "%s", file_name);
    uint32_t hpctoolkit_module_id;
    load_module_t *load_module = NULL;
    hpcrun_loadmap_lock();
    if ((load_module = hpcrun_loadmap_findByName(device_file)) == NULL) {
      hpctoolkit_module_id = hpcrun_loadModule_add(device_file);
    } else {
      hpctoolkit_module_id = load_module->id;
    }
    hpcrun_loadmap_unlock();
    cubin_module_map_entry_t *entry = cubin_module_map_lookup(cubin_module);
    if (entry == NULL) {
      Elf_SymbolVector *elf_vector = computeCubinFunctionOffsets(cubin, cubin_size);
      cubin_module_map_insert(cubin_module, cubin, hpctoolkit_module_id, elf_vector);
      cubin_module_map_insert_hash(cubin_module, hash);
    }
  }

  PRINT("Patch CUBIN\n");
  PRINT("%s\n", HPCTOOLKIT_GPU_PATCH);
  // patch binary
  HPCRUN_SANITIZER_CALL(sanitizerAddPatchesFromFile, (HPCTOOLKIT_GPU_PATCH, context));
  HPCRUN_SANITIZER_CALL(sanitizerPatchInstructions,
    (SANITIZER_INSTRUCTION_MEMORY_ACCESS, cubin_module, "sanitizer_memory_access_callback"));
  HPCRUN_SANITIZER_CALL(sanitizerPatchInstructions,
    (SANITIZER_INSTRUCTION_BLOCK_ENTER, cubin_module, "sanitizer_block_enter_callback"));
  HPCRUN_SANITIZER_CALL(sanitizerPatchInstructions,
    (SANITIZER_INSTRUCTION_BLOCK_EXIT, cubin_module, "sanitizer_block_exit_callback"));
  HPCRUN_SANITIZER_CALL(sanitizerPatchModule, (cubin_module));
}


static void
sanitizer_unload_callback
(
 const void *module,
 const void *cubin, 
 size_t cubin_size
)
{
  cubin_hash_map_delete(cubin);
  cubin_module_map_delete(module);
}

//******************************************************************************
// record handlers
//******************************************************************************

static bool
sanitizer_write_trace
(
  unsigned char *hash,
  unsigned int hash_len,
  char *trace,
  size_t trace_size
)
{
  // Write a file if does not exist, otherwise append
  // FIXME(Keren): multiple processes
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
  used += sprintf(&file_name[used], "%s", ".trace.");
  used += sprintf(&file_name[used], "%u", sanitizer_thread_id);

  int fd;
  errno = 0;
  fd = open(file_name, O_WRONLY | O_CREAT | O_APPEND, 0644);
  if (fd >= 0) {
    // Success
    if (write(fd, trace, trace_size) != trace_size) {
      close(fd);
      return false;   
    } else {
      close(fd);
      return true;
    }
  } else {
    // Failure to open is a fatal error.
    hpcrun_abort("hpctoolkit: unable to open file: '%s'", file_name);
    return false;
  }
}


static void
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


static void
sanitizer_buffer_process
(
 CUcontext context,
 CUmodule module,
 CUfunction function,
 CUstream stream,
 CUstream priority_stream,
 dim3 grid_size,
 dim3 block_size
)
{
  // look up function addr
  uint64_t function_ptr = (uint64_t)function;
  uint32_t function_index = *((uint32_t *)(function_ptr + CUFUNCTION_SYMBOL_INDEX_OFFSET));
  ip_normalized_t function_ip = cubin_module_transform(module, function_index, 0);
  uint64_t function_record = *((uint64_t *)(function_ptr + CUFUNCTION_FUNCTION_RECORD_OFFSET));
  uint64_t function_addr = *((uint64_t *)(function_record + FUNCTION_RECORD_FUNCTION_ADDR_OFFSET));

  // get a place holder cct node
  cuda_placeholder_t cuda_state = cuda_placeholders.cuda_kernel_state;
  cct_node_t *host_op_node = sanitizer_correlation_callback(cuda_state);

  // insert a function cct node
  cct_addr_t frm;
  memset(&frm, 0, sizeof(cct_addr_t));
  frm.ip_norm = function_ip;
  cct_node_t *host_function_node = hpcrun_cct_insert_addr(host_op_node, &frm);

  // first time copy back, allocate memory
  if (gpu_patch_buffer == NULL) {
    gpu_patch_buffer = (gpu_patch_buffer_t *) hpcrun_malloc_safe(sizeof(gpu_patch_buffer_t));
  }

  sanitizer_context_map_entry_t *entry = sanitizer_context_map_lookup(context);

  while (true) {
    HPCRUN_SANITIZER_CALL(sanitizerMemcpyDeviceToHost,
      (gpu_patch_buffer, gpu_patch_buffer_device, sizeof(gpu_patch_buffer_t), priority_stream));

    size_t num_records = gpu_patch_buffer->head_index;

    //PRINT("gpu_patch_buffer full %u, head_index %u, tail_index %u, size %u, num_blocks %u, block_sampling_frequency %u\n",
    //  gpu_patch_buffer->full, gpu_patch_buffer->head_index, gpu_patch_buffer->tail_index,
    //  gpu_patch_buffer->size, gpu_patch_buffer->num_blocks, gpu_patch_buffer->block_sampling_frequency);

    size_t num_left_blocks = 0;
    if (gpu_patch_buffer->block_sampling_frequency != 0) {
      size_t total = grid_size.x * grid_size.y * grid_size.z;
      num_left_blocks = total - ((total - 1) / gpu_patch_buffer->block_sampling_frequency + 1);
    }

    if (!(gpu_patch_buffer->num_blocks == num_left_blocks || gpu_patch_buffer->full) || num_records == 0) {
      // Wait until the buffer is full or the kernel is finished
      if (SANITIZER_API_DEBUG) {
        //sleep(1);
      }
      continue;
    }

    // first time copy back, allocate memory
    if (gpu_patch_record == NULL) {
      gpu_patch_record = (gpu_patch_record_t *) hpcrun_malloc_safe(
        GPU_PATCH_RECORD_NUM * sizeof(gpu_patch_record_t));
    }
    HPCRUN_SANITIZER_CALL(sanitizerMemcpyDeviceToHost,
      (gpu_patch_record, gpu_patch_buffer->records, sizeof(gpu_patch_record_t) * num_records, priority_stream));
    // copy finished
    gpu_patch_buffer->full = 0;
    // sync stream?
    HPCRUN_SANITIZER_CALL(sanitizerMemcpyHostToDeviceAsync,
      (gpu_patch_buffer_device, gpu_patch_buffer, sizeof(gpu_patch_buffer_t), priority_stream));

    if (sanitizer_trace == NULL) {
      // first time to allocate trace buffer,
      // * WARP_SIZE to guarantee sanitizer_trace buffer is greater than record size
      sanitizer_trace = (char *)
        hpcrun_malloc_safe(GPU_PATCH_RECORD_NUM * WARP_SIZE * sizeof(gpu_patch_record_t));
    }

    // XXX(Keren): tricky change offset here
    size_t used = 0;
    size_t i;
    for (i = 0; i < num_records; ++i) {
      gpu_patch_record_t *record = &(gpu_patch_record[i]);
      size_t j;
      for (j = 0; j < WARP_SIZE; ++j) {
        // Only record active values?
        if (((record->active) & (1 << j)) == 0) {
          continue;
        }
        uint64_t pc = record->pc - function_addr;
        uint32_t bid_x, bid_y, bid_z;
        uint32_t tid_x, tid_y, tid_z;
        dim3_id_transform(grid_size, record->flat_block_id, &bid_x, &bid_y, &bid_z);
        dim3_id_transform(block_size, record->flat_thread_id + j, &tid_x, &tid_y, &tid_z);
        used += sprintf(&sanitizer_trace[used], "%p|(%u,%u,%u)|(%u,%u,%u)|",
          pc, bid_x, bid_y, bid_z, tid_x, tid_y, tid_z);
        if (record->address[j]) {
          used += sprintf(&sanitizer_trace[used], "%p|", record->address[j]);
        } else {
          used += sprintf(&sanitizer_trace[used], "0x0|");
        }
        size_t k;
        used += sprintf(&sanitizer_trace[used], "0x");
        for (k = 0; k < record->size; ++k) { 
          used += sprintf(&sanitizer_trace[used], "%02x", record->value[j][k]);
        }
        if (SANITIZER_API_DEBUG) {
          used += sprintf(&sanitizer_trace[used], "|%u", record->flags[j]);
        }
        used += sprintf(&sanitizer_trace[used], "\n");
      }
    }
    used += sprintf(&sanitizer_trace[used], "\n");

    cubin_module_map_entry_t *entry = cubin_module_map_lookup(module);
    unsigned int hash_len = 0;
    unsigned char *hash = cubin_module_map_entry_hash_get(entry, &hash_len);

    if (entry != NULL) {
      PRINT("Write file size %lu\n", used);
      if (sanitizer_write_trace(hash, hash_len, sanitizer_trace, used) == false) {
        PRINT("Write file error\n");
      }
    }

    if (gpu_patch_buffer->num_blocks == num_left_blocks) {
      break;
    }
  }
}


//******************************************************************************
// callbacks
//******************************************************************************

static cct_node_t *
sanitizer_correlation_callback
(
 cuda_placeholder_t cuda_state
)
{
  hpcrun_metricVal_t zero_metric_incr = {.i = 0};
  int zero_metric_id = 0; // nothing to see here
  ucontext_t uc;
  getcontext(&uc);
  thread_data_t *td = hpcrun_get_thread_data();

  // NOTE(keren): hpcrun_safe_enter prevent self interruption
  hpcrun_safe_enter();

  cct_node_t *node = 
    hpcrun_sample_callpath(&uc, zero_metric_id, 
      zero_metric_incr, 0, 1, NULL).sample_node; 

  hpcrun_safe_exit();

  // Compress callpath
  cct_addr_t* node_addr = hpcrun_cct_addr(node);
  static __thread uint16_t libhpcrun_id = 0;

  // The lowest module must be libhpcrun, which is not 0
  // Update libhpcrun_id only the first time
  if (libhpcrun_id == 0) {
    load_module_t* module = hpcrun_loadmap_findById(node_addr->ip_norm.lm_id);
    if (module != NULL && strstr(module->name, "libhpcrun") != NULL) {
      libhpcrun_id = node_addr->ip_norm.lm_id;
    }
  }

  // Skip libhpcrun
  while (libhpcrun_id != 0 && node_addr->ip_norm.lm_id == libhpcrun_id) {
    //hpcrun_cct_delete_self(node);
    node = hpcrun_cct_parent(node);
    node_addr = hpcrun_cct_addr(node);
  }

  // Skip libcupti and libcuda
  while (module_ignore_map_module_id_lookup(node_addr->ip_norm.lm_id)) {
    //hpcrun_cct_delete_self(node);
    node = hpcrun_cct_parent(node);
    node_addr = hpcrun_cct_addr(node);
  }
  
  hpcrun_safe_enter();

  // Insert the corresponding cuda state node
  cct_addr_t frm;
  memset(&frm, 0, sizeof(cct_addr_t));
  frm.ip_norm = cuda_state.pc_norm;
  cct_node_t* cct_child = hpcrun_cct_insert_addr(node, &frm);

  hpcrun_safe_exit();

  return cct_child;
}


static void
sanitizer_kernel_launch_callback
(
 CUcontext context,
 CUmodule module,
 CUfunction function,
 CUstream stream,
 CUstream priority_stream,
 dim3 grid_size,
 dim3 block_size
)
{
  // TODO(keren): add other instrumentation mode, do not allow two threads sharing a stream
  // allocate a device memory buffer
#if 0
  cstack_node_t *buffer_device = gpu_patch_buffer_device();
  while (buffer_device == NULL) {
    uint32_t allocated_buffer_num = atomic_load(&sanitizer_allocated_buffer_num);
    if (allocated_buffer_num >= GPU_PATCH_BUFFER_POOL_NUM) {
      // Wait for previous buffer to be finished
      // Use a background thread to handle other context
      sanitizer_context_map_context_process(context, sanitizer_buffer_dispatch);
    } else {
      if (atomic_compare_exchange_strong(&sanitizer_allocated_buffer_num,
        &allocated_buffer_num, allocated_buffer_num + 1)) {
        // allocate a new one
        break;
      }
    }
    buffer_device = gpu_patch_buffer_device();
  }
  sanitizer_entry_buffer_t *buffer_device_entry = NULL;

#endif


  if (gpu_patch_buffer_device == NULL) {
    // allocate buffer
    void *gpu_patch_records = NULL;

    // gpu_patch_buffer
    HPCRUN_SANITIZER_CALL(sanitizerAlloc, (&(gpu_patch_buffer_device), sizeof(gpu_patch_buffer_t)));
    HPCRUN_SANITIZER_CALL(sanitizerMemset, (gpu_patch_buffer_device, 0, sizeof(gpu_patch_buffer_t), stream));

    PRINT("Allocate gpu_patch_buffer %p, size %zu\n", gpu_patch_buffer_device, sizeof(gpu_patch_buffer_t));

    // gpu_patch_buffer_t->records
    HPCRUN_SANITIZER_CALL(sanitizerAlloc,
      (&gpu_patch_records, GPU_PATCH_RECORD_NUM * sizeof(gpu_patch_record_t)));
    HPCRUN_SANITIZER_CALL(sanitizerMemset,
      (gpu_patch_records, 0, GPU_PATCH_RECORD_NUM * sizeof(gpu_patch_record_t), stream));

    PRINT("Allocate gpu_patch_records %p, size %zu\n", gpu_patch_records, GPU_PATCH_RECORD_NUM * sizeof(gpu_patch_record_t));

    gpu_patch_buffer_reset.records = gpu_patch_records;
    gpu_patch_buffer_reset.num_blocks = grid_size.x * grid_size.y * grid_size.z;
    gpu_patch_buffer_reset.block_sampling_frequency = sanitizer_block_sampling_frequency_get();
    PRINT("Sampling frequency %d\n", sanitizer_block_sampling_frequency_get());
    HPCRUN_SANITIZER_CALL(sanitizerMemcpyHostToDeviceAsync,
      (gpu_patch_buffer_device, &gpu_patch_buffer_reset, sizeof(gpu_patch_buffer_t), stream));
  } else {
    // reset buffer
    gpu_patch_buffer_reset.num_blocks = grid_size.x * grid_size.y * grid_size.z;
    HPCRUN_SANITIZER_CALL(sanitizerMemcpyHostToDeviceAsync,
      (gpu_patch_buffer_device, &gpu_patch_buffer_reset,
       sizeof(gpu_patch_buffer_t) - sizeof(void *), stream));
  }

  HPCRUN_SANITIZER_CALL(sanitizerSetCallbackData, (stream, gpu_patch_buffer_device));
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
    sanitizer_thread_id = atomic_fetch_add(&sanitizer_global_thread_id, 1);
    sanitizer_stop_flag_set();
  }
  // XXX(keren): assume single thread per stream
  if (domain == SANITIZER_CB_DOMAIN_RESOURCE) {
    switch (cbid) {
      case SANITIZER_CBID_RESOURCE_MODULE_LOADED:
        {
          // single thread
          Sanitizer_ResourceModuleData *md = (Sanitizer_ResourceModuleData *)cbdata;
          sanitizer_load_callback(md->context, md->module, md->pCubin, md->cubinSize);
          break;
        }
      case SANITIZER_CBID_RESOURCE_MODULE_UNLOAD_STARTING:
        {
          // single thread
          Sanitizer_ResourceModuleData *md = (Sanitizer_ResourceModuleData *)cbdata;
          sanitizer_unload_callback(md->module, md->pCubin, md->cubinSize);
          break;
        }
      case SANITIZER_CBID_RESOURCE_STREAM_CREATED:
        {
          // single thread
          // TODO
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
          PRINT("Allocate memory address %p, size %zu\n", md->address, md->size);
          break;
        }
      case SANITIZER_CBID_RESOURCE_DEVICE_MEMORY_FREE:
        {
          Sanitizer_ResourceMemoryData *md = (Sanitizer_ResourceMemoryData *)cbdata;
          PRINT("Free memory address %p, size %zu\n", md->address, md->size);
          break;
        }
      default:
        {
          break;
        }
    }
  } else if (domain == SANITIZER_CB_DOMAIN_LAUNCH) {
    Sanitizer_LaunchData *ld = (Sanitizer_LaunchData *)cbdata;
    if (cbid == SANITIZER_CBID_LAUNCH_BEGIN) {
      grid_size.x = ld->gridDim_x;
      grid_size.y = ld->gridDim_y;
      grid_size.z = ld->gridDim_z;
      block_size.x = ld->blockDim_x;
      block_size.y = ld->blockDim_y;
      block_size.z = ld->blockDim_z;
      PRINT("Launch kernel %s <%d, %d, %d>:<%d, %d, %d>\n", ld->functionName,
        ld->gridDim_x, ld->gridDim_y, ld->gridDim_z, ld->blockDim_x, ld->blockDim_y, ld->blockDim_z);
      // multi-thread
      // gaurantee that each time only a single callback data is associated with a stream
      sanitizer_context_map_stream_lock(ld->context, ld->stream);
      sanitizer_context_map_entry_t *entry = sanitizer_context_map_lookup(ld->context);
      CUstream priority_stream = NULL;
      // Create a high priority stream for the context at the first time
      if (entry == NULL || sanitizer_context_map_entry_priority_stream_get(entry) == NULL) {
        priority_stream = cuda_priority_stream_create();
        sanitizer_context_map_init(ld->context, priority_stream);
      } else {
        priority_stream = sanitizer_context_map_entry_priority_stream_get(entry);
      }
      sanitizer_kernel_launch_callback(ld->context, ld->module, ld->function, ld->stream,
        priority_stream, grid_size, block_size);
    } else if (cbid == SANITIZER_CBID_LAUNCH_END) {
      sanitizer_context_map_entry_t *entry = sanitizer_context_map_lookup(ld->context);
      CUstream priority_stream = NULL;
      // Create a high priority stream for the context at the first time
      if (entry == NULL || sanitizer_context_map_entry_priority_stream_get(entry) == NULL) {
        priority_stream = cuda_priority_stream_create();
        sanitizer_context_map_init(ld->context, priority_stream);
      } else {
        priority_stream = sanitizer_context_map_entry_priority_stream_get(entry);
      }
      sanitizer_buffer_process(ld->context, ld->module, ld->function, ld->stream,
        priority_stream, grid_size, block_size);
      sanitizer_context_map_stream_unlock(ld->context, ld->stream);
    }
  } else if (domain == SANITIZER_CB_DOMAIN_MEMCPY) {
    // TODO(keren): variable correaltion and sync data
  } else if (domain == SANITIZER_CB_DOMAIN_SYNCHRONIZE) {
  #if 0
    Sanitizer_SynchronizeData *sd = (Sanitizer_SynchronizeData*)cbdata;
    if (cbid == SANITIZER_CBID_SYNCHRONIZE_STREAM_SYNCHRONIZED) {
      // multi-thread
      PRINT("Synchronize stream %p\n", sd->stream);
      sanitizer_context_map_stream_process(sd->context, sd->stream, sanitizer_buffer_dispatch);
    } else if (cbid == SANITIZER_CBID_SYNCHRONIZE_CONTEXT_SYNCHRONIZED) {
      // multi-thread
      PRINT("Synchronize context %p\n", sd->context);
      sanitizer_context_map_context_process(sd->context, sanitizer_buffer_dispatch);
    }
  #endif
  }
}


//******************************************************************************
// interfaces
//******************************************************************************

bool
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


void
sanitizer_callbacks_subscribe() 
{
  HPCRUN_SANITIZER_CALL(sanitizerSubscribe,
    (&sanitizer_subscriber_handle, sanitizer_subscribe_callback, NULL));

  HPCRUN_SANITIZER_CALL(sanitizerEnableDomain,
    (1, sanitizer_subscriber_handle, SANITIZER_CB_DOMAIN_RESOURCE));

  HPCRUN_SANITIZER_CALL(sanitizerEnableDomain,
    (1, sanitizer_subscriber_handle, SANITIZER_CB_DOMAIN_SYNCHRONIZE));

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
  HPCRUN_SANITIZER_CALL(sanitizerUnsubscribe, (sanitizer_subscriber_handle));

  HPCRUN_SANITIZER_CALL(sanitizerEnableDomain,
    (0, sanitizer_subscriber_handle, SANITIZER_CB_DOMAIN_RESOURCE));

  HPCRUN_SANITIZER_CALL(sanitizerEnableDomain,
    (0, sanitizer_subscriber_handle, SANITIZER_CB_DOMAIN_SYNCHRONIZE));

  HPCRUN_SANITIZER_CALL(sanitizerEnableDomain,
    (0, sanitizer_subscriber_handle, SANITIZER_CB_DOMAIN_LAUNCH));

  HPCRUN_SANITIZER_CALL(sanitizerEnableDomain,
    (0, sanitizer_subscriber_handle, SANITIZER_CB_DOMAIN_MEMCPY));

  HPCRUN_SANITIZER_CALL(sanitizerEnableDomain,
    (0, sanitizer_subscriber_handle, SANITIZER_CB_DOMAIN_MEMSET));
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
  }
}


void
sanitizer_device_shutdown(void *args)
{
  sanitizer_callbacks_unsubscribe();
}

//
//static _Atomic(bool) stop_trace_flag;
//
//#define SECONDS_UNTIL_WAKEUP 1
//
//typedef sanitizer_trace_record_t {
//  pthread_mutex_t mutex;
//  pthread_cond_t cond;
//  cudaStream_t stream;
//};
//
//
//void
//sanitizer_trace_activities_await
//(
// pthread_cond_t *cond,
// pthread_mutex_t *mutex
//)
//{
//  struct timespec time;
//  clock_gettime(CLOCK_REALTIME, &time); // get current time
//  time.tv_sec += SECONDS_UNTIL_WAKEUP;
//
//  // wait for a signal or for a few seconds. periodically waking
//  // up avoids missing a signal.
//  pthread_cond_timedwait(cond, mutex, &time); 
//}
//
//
//void
//sanitizer_trace_activities_process
//(
// cudaStream_t stream
//)
//{
//  while (gpu_patch_queue_empty()) {
//    gpu_patch_queue_pop(); 
//
//
//    sanitizer_trace_activities_await();
//  }
//}
//
//
//void *
//sanitizer_trace_thread
//(
// cudaStream_t *stream
//)
//{
//  while (!atomic_load(&stop_trace_flag)) {
//    sanitizer_trace_activities_process(stream);
//    sanitizer_trace_activities_await(cond, mutex);
//  }
//
//  sanitizer_trace_activities_process(stream);
//
//  return NULL;
//}
//
//
//void
//sanitizer_trace_init()
//{
//  cudaStream_t stream = cuda_create_priority_stream();
//
//  // Create a new thread for the stream without libmonitor watching
//  monitor_disable_new_threads();
//
//  pthread_create(&trace->thread, NULL, (pthread_start_routine_t) sanitizer_trace_thread, 
//		&stream);
//
//  monitor_enable_new_threads();
//}
