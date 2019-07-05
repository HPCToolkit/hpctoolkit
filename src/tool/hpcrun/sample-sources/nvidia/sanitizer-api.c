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
#include "cstack.h"

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

#define SANITIZER_BUFFER_SIZE 64 * 1024
#define SANITIZER_THREAD_HASH_SIZE (128 * 1024 - 1)

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
static spinlock_t files_lock = SPINLOCK_UNLOCKED;
static Sanitizer_SubscriberHandle sanitizer_subscriber_handle;

static __thread bool sanitizer_stop_flag = false;
static __thread uint32_t sanitizer_thread_id = 0;
static atomic_uint sanitizer_global_thread_id = ATOMIC_VAR_INIT(0);

static __thread sanitizer_buffer_t buffer_reset = {
 .cur_index = 0,
 .max_index = SANITIZER_BUFFER_SIZE
};
static __thread sanitizer_memory_buffer_t *memory_buffer_host = NULL;
static __thread sanitizer_buffer_t *buffer_host = NULL;

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
  const char *path = "libsanitizer-public.so";

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
  spinlock_lock(&files_lock);
  file_flag = sanitizer_write_cubin(file_name, cubin, cubin_size);
  spinlock_unlock(&files_lock);

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
      cubin_module_map_insert(cubin_module, hpctoolkit_module_id, elf_vector);
    }
  }

  // patch binary
  HPCRUN_SANITIZER_CALL(sanitizerAddPatchesFromFile, ("./memory.fatbin", context));
  HPCRUN_SANITIZER_CALL(sanitizerPatchInstructions,
    (SANITIZER_INSTRUCTION_BARRIER, cubin_module, "sanitizer_barrier_callback"));
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

static void
sanitizer_memory_process
(
 CUmodule module,
 CUstream stream,
 uint32_t function_index,
 cct_node_t *host_op_node,
 void *buffers,
 size_t num_buffers
)
{
  if (memory_buffer_host == NULL) {
    // first time copy back, allocate memory
    memory_buffer_host = (sanitizer_memory_buffer_t *)
      hpcrun_malloc(SANITIZER_BUFFER_SIZE * sizeof(sanitizer_memory_buffer_t));
  }
  HPCRUN_SANITIZER_CALL(sanitizerMemcpyDeviceToHost,
    (memory_buffer_host, buffers, sizeof(sanitizer_memory_buffer_t) * num_buffers, stream));

  // do analysis here
  sanitizer_activity_t activity;
  activity.type = SANITIZER_ACTIVITY_TYPE_MEMORY;

  // attribute properties back to cct tree
  size_t i;
  // XXX(Keren): tricky change offset here
  uint64_t pc_offset = 0x0;
  for (i = 0; i < num_buffers; ++i) {
     sanitizer_memory_buffer_t *memory_buffer = &(memory_buffer_host[i]);
     ip_normalized_t ip = cubin_module_transform(module, function_index,
       memory_buffer->pc - memory_buffer_host[0].pc + pc_offset);
     cct_addr_t frm = { .ip_norm = ip };
     cct_node_t *cct_child = NULL;
     if ((cct_child = hpcrun_cct_insert_addr(host_op_node, &frm)) != NULL) {
       //sanitizer_activity_attribute(&activity, cct_child);
     }
     PRINT("<%u, %u, %u>, <%u, %u, %u>: pc %p, size %u, flags %u, address %p\n",
       memory_buffer->thread_ids.x, memory_buffer->thread_ids.y, memory_buffer->thread_ids.z,
       memory_buffer->block_ids.x, memory_buffer->block_ids.y, memory_buffer->block_ids.z,
       memory_buffer->pc, memory_buffer->size, memory_buffer->flags, memory_buffer->address);
  }
}


static void
sanitizer_buffer_process
(
 cstack_node_t *node
)
{
  sanitizer_entry_notification_t *notification = node->entry;
  cct_node_t *host_op_node = notification->host_op_node;
  CUmodule module = notification->module;
  CUstream stream = notification->stream;
  uint32_t function_index = notification->function_index;
  sanitizer_activity_type_t type = notification->type;
  cstack_node_t *buffer_device = notification->buffer_device;
  sanitizer_entry_buffer_t *buffer_device_entry = (sanitizer_entry_buffer_t *)buffer_device->entry;

  // first time copy back, allocate memory
  if (buffer_host == NULL) {
    buffer_host = (sanitizer_buffer_t *) hpcrun_malloc(sizeof(sanitizer_buffer_t));
  }
  HPCRUN_SANITIZER_CALL(sanitizerMemcpyDeviceToHost,
    (buffer_host, buffer_device_entry->buffer, sizeof(sanitizer_buffer_t), stream));
  size_t num_buffers = MIN2(buffer_host->cur_index, buffer_host->max_index);
  PRINT("sanitizer_memory_process->num_buffers %lu\n", num_buffers);

  switch (type) {
    case SANITIZER_ACTIVITY_TYPE_MEMORY:
      {
        sanitizer_memory_process(module, stream, function_index, host_op_node, 
          buffer_host->buffers, num_buffers);
        sanitizer_buffer_device_push(SANITIZER_ACTIVITY_TYPE_MEMORY, buffer_device);
        break;
      }
    default:
      break;
  }
}


static void
sanitizer_buffer_dispatch
(
 cstack_t *stack
)
{
  sanitizer_notification_apply(stack, sanitizer_buffer_process);
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
 CUstream stream,
 const char *function_name
)
{
  // look up function index
  cubin_module_map_entry_t *entry = cubin_module_map_lookup(module);
  bool find = false;
  uint32_t function_index = 0;
  if (entry != NULL) {
    Elf_SymbolVector *elf_vector = cubin_module_map_entry_elf_vector_get(entry);
    size_t i;
    for (i = 0; i < elf_vector->nsymbols; ++i) {
      if (elf_vector->names[i] != NULL) {
        if (strcmp(elf_vector->names[i], function_name) == 0) {
          find = true;
          function_index = i;
          PRINT("function %s function_index %d\n", function_name, function_index);
          break;
        }
      }
    }
  }

  if (find == false) {
    PRINT("function not found %s\n", function_name);
    return;
  }

  // TODO(keren): add other instrumentation mode, do not allow two threads sharing a stream
  // allocate a device memory buffer
  cstack_node_t *buffer_device = sanitizer_buffer_device_pop(SANITIZER_ACTIVITY_TYPE_MEMORY);
  sanitizer_entry_buffer_t *buffer_device_entry = NULL;

  if (buffer_device == NULL) {
    buffer_device = sanitizer_buffer_node_new(NULL);
    buffer_device_entry = (sanitizer_entry_buffer_t *)buffer_device->entry;
    // allocate buffer
    void *memory_buffer_device = NULL;
    void **prev_buffer_device = NULL;
    uint32_t *hash_buffer_device = NULL;

    // sanitizer_buffer
    HPCRUN_SANITIZER_CALL(sanitizerAlloc, (&(buffer_device_entry->buffer), sizeof(sanitizer_buffer_t)));
    HPCRUN_SANITIZER_CALL(sanitizerMemset, (buffer_device_entry->buffer, 0, sizeof(sanitizer_buffer_t), stream));

    // sanitizer_buffer_t->thread_has_locks
    HPCRUN_SANITIZER_CALL(sanitizerAlloc, (&hash_buffer_device,
      SANITIZER_THREAD_HASH_SIZE * sizeof(uint32_t)));
    HPCRUN_SANITIZER_CALL(sanitizerMemset, (hash_buffer_device, 0,
      SANITIZER_THREAD_HASH_SIZE * sizeof(uint32_t), stream));

    PRINT("Allocate hash_buffer_device %p\n", hash_buffer_device);

    // sanitizer_buffer_t->prev_buffers
    HPCRUN_SANITIZER_CALL(sanitizerAlloc, (&prev_buffer_device,
      SANITIZER_THREAD_HASH_SIZE * sizeof(void *)));
    HPCRUN_SANITIZER_CALL(sanitizerMemset, (prev_buffer_device, 0,
      SANITIZER_THREAD_HASH_SIZE * sizeof(void *), stream));

    // sanitizer_buffer_t->buffers
    HPCRUN_SANITIZER_CALL(sanitizerAlloc,
      (&memory_buffer_device, SANITIZER_BUFFER_SIZE * sizeof(sanitizer_memory_buffer_t)));
    HPCRUN_SANITIZER_CALL(sanitizerMemset,
      (memory_buffer_device, 0, SANITIZER_BUFFER_SIZE * sizeof(sanitizer_memory_buffer_t), stream));

    buffer_reset.thread_hash_locks = hash_buffer_device;
    buffer_reset.prev_buffers = prev_buffer_device;
    buffer_reset.buffers = memory_buffer_device;
    HPCRUN_SANITIZER_CALL(sanitizerMemcpyHostToDeviceAsync,
      (buffer_device_entry->buffer, &buffer_reset, sizeof(sanitizer_buffer_t), stream));
  } else {
    buffer_device_entry = (sanitizer_entry_buffer_t *)buffer_device->entry;
    // reset buffer
    HPCRUN_SANITIZER_CALL(sanitizerMemcpyHostToDeviceAsync,
      (buffer_device_entry->buffer, &buffer_reset, sizeof(uint32_t) * 2, stream));
  }

  HPCRUN_SANITIZER_CALL(sanitizerSetCallbackData, (stream, buffer_device_entry->buffer));

  // get cct_node
  cuda_placeholder_t cuda_state = cuda_placeholders.cuda_kernel_state;
  cct_node_t *host_op_node = sanitizer_correlation_callback(cuda_state);

  // correlte record with stream
  sanitizer_notification_insert(context, module, stream, function_index,
    SANITIZER_ACTIVITY_TYPE_MEMORY, buffer_device, sanitizer_thread_id, host_op_node);
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
          PRINT("stream destroy starting\n");
          break;
        }
      case SANITIZER_CBID_RESOURCE_CONTEXT_CREATION_FINISHED:
        {
          // single thread
          // TODO
          break;
        }
      case SANITIZER_CBID_RESOURCE_CONTEXT_DESTROY_STARTING:
        {
          // single thread
          // TODO
          PRINT("context destroy starting\n");
          break;
        }
      case SANITIZER_CBID_RESOURCE_DEVICE_MEMORY_ALLOC:
        {
          //TODO
          break;
        }
      case SANITIZER_CBID_RESOURCE_DEVICE_MEMORY_FREE:
        {
          //TODO
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
      // multi-thread
      sanitizer_kernel_launch_callback(ld->context, ld->module, ld->stream, ld->functionName);
    }
  } else if (domain == SANITIZER_CB_DOMAIN_MEMCPY) {
    // TODO(keren): variable correaltion and sync data
  } else if (domain == SANITIZER_CB_DOMAIN_SYNCHRONIZE) {
    Sanitizer_SynchronizeData *sd = (Sanitizer_SynchronizeData*)cbdata;
    if (cbid == SANITIZER_CBID_SYNCHRONIZE_STREAM_SYNCHRONIZED) {
      // multi-thread
      sanitizer_context_map_stream_process(sd->context, sd->stream, sanitizer_buffer_dispatch);
    } else if (cbid == SANITIZER_CBID_SYNCHRONIZE_CONTEXT_SYNCHRONIZED) {
      // multi-thread
      sanitizer_context_map_context_process(sd->context, sanitizer_buffer_dispatch);
    }
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
    sanitizer_context_map_process(sanitizer_buffer_dispatch);
  }
}


void
sanitizer_device_shutdown(void *args)
{
  sanitizer_callbacks_unsubscribe();
}
