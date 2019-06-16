#include <stdio.h>
#include <errno.h>     // errno
#include <fcntl.h>     // open
#include <limits.h>    // PATH_MAX
// #include <pthread.h>
#include <stdio.h>     // sprintf
#include <unistd.h>
#include <sys/stat.h>  // mkdir
#include <string.h>        // strstr

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

#include "cuda-device-map.h"
#include "cuda-state-placeholders.h"

#include "cubin-id-map.h"
#include "cubin-hash-map.h"

#include "sanitizer-api.h"
#include "sanitizer-node.h"

#define SANITIZER_API_DEBUG 1

#if SANITIZER_API_DEBUG
#define PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define PRINT(...)
#endif

#define MIN2(m1, m2) m1 > m2 ? m2 : m1

static Sanitizer_SubscriberHandle sanitizer_subscriber_handle;
static cct_node_t *host_op_node;
static CUstream stream;
static uint32_t cubin_id = 0;
static uint32_t function_index = 0;
static Elf_SymbolVector *elf_vector = NULL;
static spinlock_t files_lock = SPINLOCK_UNLOCKED;
static __thread bool sanitizer_stop_flag = false;

bool
sanitizer_bind()
{
  return false;
}

//******************************************************************************
// private operations
//******************************************************************************

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
 CUmodule module, 
 const void *cubin, 
 size_t cubin_size
)
{
  // Compute hash for cubin and store it into a map
  cubin_hash_map_entry_t *entry = cubin_hash_map_lookup(cubin_id);
  unsigned char *hash;
  unsigned int hash_len;
  if (entry == NULL) {
    cubin_hash_map_insert(cubin_id, cubin, cubin_size);
    entry = cubin_hash_map_lookup(cubin_id);
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
    load_module_t *module = NULL;
    hpcrun_loadmap_lock();
    if ((module = hpcrun_loadmap_findByName(device_file)) == NULL) {
      hpctoolkit_module_id = hpcrun_loadModule_add(device_file);
    } else {
      hpctoolkit_module_id = module->id;
    }
    hpcrun_loadmap_unlock();
    //cubin_id_map_entry_t *entry = cubin_id_map_lookup(module_id);
    //if (entry == NULL) {
    elf_vector = computeCubinFunctionOffsets(cubin, cubin_size);
    cubin_id_map_insert(cubin_id, hpctoolkit_module_id, elf_vector);
    //}
  }

  sanitizerAddPatchesFromFile("/home/kz21/Codes/hpctoolkit-gpu-samples/memory.fatbin", 0);
  sanitizerPatchInstructions(SANITIZER_INSTRUCTION_MEMORY_ACCESS, module, "MemoryAccessCallback");
  sanitizerPatchModule(module);
}


static void
sanitizer_unload_callback
(
 int module_id, 
 const void *cubin, 
 size_t cubin_size
)
{
  //cubin_id_map_delete(module_id);
}


#define SANITIZER_BUFFER_SIZE 10240
static sanitizer_memory_t sanitizer_memory_reset = {
 .cur_index = 0,
 .max_index = SANITIZER_BUFFER_SIZE,
};
static sanitizer_memory_record_t *sanitizer_memory_host_records = NULL;
static sanitizer_memory_t *sanitizer_memory_host = NULL;
static sanitizer_memory_t *sanitizer_memory_device = NULL;


//******************************************************************************
// record handlers
//******************************************************************************

static void
sanitizer_record_process
(
)
{
  if (sanitizer_memory_host == NULL) {
    sanitizer_memory_host = (sanitizer_memory_t *) hpcrun_malloc(sizeof(sanitizer_memory_t));
    sanitizer_memory_host_records = (sanitizer_memory_record_t *) hpcrun_malloc(
      sizeof(sanitizer_memory_record_t) * SANITIZER_BUFFER_SIZE);
  }

  size_t num_records = 0;
  sanitizerMemcpyDeviceToHost(sanitizer_memory_host, sanitizer_memory_device,
    sizeof(sanitizer_memory_t), stream);

  num_records = MIN2(sanitizer_memory_host->cur_index, sanitizer_memory_host->max_index);
  PRINT("num_records %d\n", num_records);
  sanitizerMemcpyDeviceToHost(sanitizer_memory_host_records, sanitizer_memory_host->records,
    sizeof(sanitizer_memory_record_t) * num_records, stream);

  size_t i;
  for (i = 0; i < num_records; ++i) {
     sanitizer_memory_record_t *record = &(sanitizer_memory_host_records[i]);
     ip_normalized_t ip = cubin_id_transform(cubin_id, function_index, record->pc - sanitizer_memory_host_records[0].pc + 0xE0);
     PRINT("pc %p\n", record->pc - sanitizer_memory_host_records[0].pc + 0xE0);
     cct_addr_t frm = { .ip_norm = ip };
     cct_node_t *cct_child = NULL;
     if ((cct_child = hpcrun_cct_insert_addr(host_op_node, &frm)) != NULL) {
       sanitizer_record_attribute(cct_child);
     }
  }
}

//******************************************************************************
// callbacks
//******************************************************************************

static void
sanitizer_kernel_launch_callback
(
 Sanitizer_LaunchData *ld
)
{
  // alloc memory array
  if (sanitizer_memory_device == NULL) {
    sanitizer_memory_record_t *records;
    sanitizerAlloc((void**)&(records), sizeof(sanitizer_memory_record_t) *
      SANITIZER_BUFFER_SIZE);
    sanitizerMemset(records, 0, sizeof(sanitizer_memory_record_t) *
      SANITIZER_BUFFER_SIZE, stream);
    sanitizer_memory_reset.records = records;
    sanitizerAlloc((void**)&sanitizer_memory_device, sizeof(sanitizer_memory_t));
  }
  // reset memory array
  sanitizerMemcpyHostToDeviceAsync(sanitizer_memory_device, &sanitizer_memory_reset,
    sizeof(sanitizer_memory_t), stream);

  sanitizerSetCallbackData(stream, sanitizer_memory_device);
}


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
  td->overhead++;
  hpcrun_safe_enter();

  cct_node_t *node = 
    hpcrun_sample_callpath(&uc, zero_metric_id, 
			   zero_metric_incr, 0, 1, NULL).sample_node; 

  hpcrun_safe_exit();
  td->overhead--;

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
  
  td->overhead++;
  hpcrun_safe_enter();

  // Insert the corresponding cuda state node
  cct_addr_t frm;
  memset(&frm, 0, sizeof(cct_addr_t));
  frm.ip_norm = cuda_state.pc_norm;
  cct_node_t* cct_child = hpcrun_cct_insert_addr(node, &frm);

  hpcrun_safe_exit();
  td->overhead--;

  return cct_child;
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
  // XXX(keren): assume single thread, single context, and single stream
  if (domain == SANITIZER_CB_DOMAIN_RESOURCE) {
    if (cbid == SANITIZER_CBID_RESOURCE_MODULE_LOADED) {
      Sanitizer_ResourceModuleData* rd = (Sanitizer_ResourceModuleData*)cbdata;
      sanitizer_load_callback(rd->module, rd->pCubin, rd->cubinSize);
    } else if (cbid == SANITIZER_CBID_RESOURCE_MODULE_UNLOAD) {
      Sanitizer_ResourceModuleData* rd = (Sanitizer_ResourceModuleData*)cbdata;
      sanitizer_unload_callback(rd->module, rd->pCubin, rd->cubinSize);
    }
  } else if (domain == SANITIZER_CB_DOMAIN_LAUNCH) {
    sanitizer_stop_flag_set();
    if (cbid == SANITIZER_CBID_LAUNCH_BEGIN) {
      Sanitizer_LaunchData* ld = (Sanitizer_LaunchData*)cbdata;
      stream = ld->stream;
      size_t i;
      for (i = 0; i < elf_vector->nsymbols; ++i) {
        if (elf_vector->names[i] != NULL) {
          if (strcmp(elf_vector->names[i], ld->functionName) == 0) {
            function_index = i;
            PRINT("function_index %d\n", function_index);
            break;
          }
        }
      }
      sanitizer_kernel_launch_callback(ld);
      cuda_placeholder_t cuda_state = cuda_placeholders.cuda_kernel_state;
      host_op_node = sanitizer_correlation_callback(cuda_state);
    }
  } else if (domain == SANITIZER_CB_DOMAIN_SYNCHRONIZE) {
    sanitizer_stop_flag_set();
    if (cbid == SANITIZER_CBID_SYNCHRONIZE_STREAM_SYNCHRONIZED) {
      //sanitizer_record_process();
    } else if (cbid == SANITIZER_CBID_SYNCHRONIZE_CONTEXT_SYNCHRONIZED) {
      //sanitizer_record_process();
    }
  }
}


void
sanitizer_callbacks_subscribe() 
{
  sanitizerSubscribe(&sanitizer_subscriber_handle, sanitizer_subscribe_callback, NULL);
  sanitizerEnableAllDomains(1, sanitizer_subscriber_handle);
}


void
sanitizer_callbacks_unsubscribe() 
{
  sanitizerUnsubscribe(sanitizer_subscriber_handle);
  sanitizerEnableAllDomains(0, sanitizer_subscriber_handle);
}


//******************************************************************************
// finalizer
//******************************************************************************

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
    //sanitizer_record_process();
  }
}


void
sanitizer_device_shutdown(void *args)
{
  sanitizer_record_process();
  //sanitizer_callbacks_unsubscribe();
}
