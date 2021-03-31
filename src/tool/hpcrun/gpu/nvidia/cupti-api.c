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
// Copyright ((c)) 2002-2021, Rice University
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
//   cupti-api.c
//
// Purpose:
//   implementation of wrapper around NVIDIA's CUPTI performance tools API
//
//***************************************************************************

//***************************************************************************
// system includes
//***************************************************************************

#include <errno.h>     // errno
#include <fcntl.h>     // open
#include <sys/stat.h>  // mkdir
#include <sys/types.h>
#include <unistd.h>

#ifndef HPCRUN_STATIC_LINK
#include <dlfcn.h>
#undef _GNU_SOURCE
#define _GNU_SOURCE
#include <link.h>          // dl_iterate_phdr
#include <linux/limits.h>  // PATH_MAX
#include <string.h>        // strstr
#endif



//***************************************************************************
// local includes
//***************************************************************************

#include <include/gpu-binary.h>

#include <lib/prof-lean/spinlock.h>

#include <hpcrun/files.h>
#include <hpcrun/hpcrun_stats.h>
#include <hpcrun/main.h> // hpcrun_force_dlopen
#include <hpcrun/safe-sampling.h>

#include <hpcrun/gpu/gpu-activity-channel.h>
#include <hpcrun/gpu/gpu-application-thread-api.h>
#include <hpcrun/gpu/gpu-monitoring-thread-api.h>
#include <hpcrun/gpu/gpu-correlation-channel.h>
#include <hpcrun/gpu/gpu-correlation-id.h>
#include <hpcrun/gpu/gpu-op-placeholders.h>

#include <hpcrun/ompt/ompt-device.h>

#include <hpcrun/sample-sources/libdl.h>
#include <hpcrun/sample-sources/nvidia.h>

#include <hpcrun/utilities/hpcrun-nanotime.h>

#include "cuda-api.h"
#include "cupti-api.h"
#include "cupti-gpu-api.h"
#include "cubin-hash-map.h"
#include "cubin-id-map.h"



//******************************************************************************
// macros
//******************************************************************************

#define CUPTI_LIBRARY_LOCATION "lib64/libcupti.so"
#define CUPTI_PATH_FROM_CUDA "extras/CUPTI/"

#define HPCRUN_CUPTI_ACTIVITY_BUFFER_SIZE (16 * 1024 * 1024)
#define HPCRUN_CUPTI_ACTIVITY_BUFFER_ALIGNMENT (8)

#define CUPTI_FN_NAME(f) DYN_FN_NAME(f)

#define CUPTI_FN(fn, args) \
  static CUptiResult (*CUPTI_FN_NAME(fn)) args

#define HPCRUN_CUPTI_CALL(fn, args)  \
{  \
  CUptiResult status = CUPTI_FN_NAME(fn) args;  \
  if (status != CUPTI_SUCCESS) {  \
    cupti_error_report(status, #fn);  \
  }  \
}

#define DISPATCH_CALLBACK(fn, args) if (fn) fn args

#define FORALL_CUPTI_ROUTINES(macro)             \
  macro(cuptiActivityConfigurePCSampling)        \
  macro(cuptiActivityDisable)                    \
  macro(cuptiActivityDisableContext)             \
  macro(cuptiActivityEnable)                     \
  macro(cuptiActivityEnableContext)              \
  macro(cuptiActivityFlushAll)                   \
  macro(cuptiActivitySetAttribute)               \
  macro(cuptiActivityGetNextRecord)              \
  macro(cuptiActivityGetNumDroppedRecords)       \
  macro(cuptiActivityPopExternalCorrelationId)   \
  macro(cuptiActivityPushExternalCorrelationId)  \
  macro(cuptiActivityRegisterCallbacks)          \
  macro(cuptiGetTimestamp)                       \
  macro(cuptiEnableDomain)                       \
  macro(cuptiFinalize)                           \
  macro(cuptiGetResultString)                    \
  macro(cuptiSubscribe)                          \
  macro(cuptiUnsubscribe)



//******************************************************************************
// types
//******************************************************************************

typedef void (*cupti_error_callback_t)
(
 const char *type,
 const char *fn,
 const char *error_string
);


typedef CUptiResult (*cupti_activity_enable_t)
(
 CUpti_ActivityKind activity
);


typedef cct_node_t *(*cupti_correlation_callback_t)
(
 uint64_t id
);


typedef void (*cupti_load_callback_t)
(
 uint32_t cubin_id,
 const void *cubin,
 size_t cubin_size
);


typedef struct {
  CUpti_BuffersCallbackRequestFunc buffer_request;
  CUpti_BuffersCallbackCompleteFunc buffer_complete;
} cupti_activity_buffer_state_t;


// DRIVER_UPDATE_CHECK(Keren): cufunc and cumod fields are reverse engineered
typedef struct {
  uint32_t unknown_field1[4];
  uint32_t function_index;
  uint32_t unknown_field2[3];
  CUmodule cumod;
} hpctoolkit_cufunc_st_t;


typedef struct {
  uint32_t cubin_id;
} hpctoolkit_cumod_st_t;


//******************************************************************************
// forward declarations
//******************************************************************************

static void
cupti_error_callback_dummy
(
 const char *type,
 const char *fn,
 const char *error_string
);


static cct_node_t *
cupti_correlation_callback_dummy
(
 uint64_t id
);



//******************************************************************************
// static data
//******************************************************************************

static spinlock_t files_lock = SPINLOCK_UNLOCKED;

static __thread bool cupti_stop_flag = false;
static __thread bool cupti_runtime_api_flag = false;
static __thread cct_node_t *cupti_kernel_ph = NULL;
static __thread cct_node_t *cupti_trace_ph = NULL;

static bool cupti_correlation_enabled = false;
static bool cupti_pc_sampling_enabled = false;

static cupti_correlation_callback_t cupti_correlation_callback =
  cupti_correlation_callback_dummy;

static cupti_error_callback_t cupti_error_callback =
  cupti_error_callback_dummy;

static cupti_activity_buffer_state_t cupti_activity_enabled = { 0, 0 };

static cupti_load_callback_t cupti_load_callback = 0;

static cupti_load_callback_t cupti_unload_callback = 0;


static CUpti_SubscriberHandle cupti_subscriber;


//----------------------------------------------------------
// cupti function pointers for late binding
//----------------------------------------------------------

CUPTI_FN
(
 cuptiActivityEnable,
 (
  CUpti_ActivityKind kind
 )
);


CUPTI_FN
(
 cuptiActivityDisable,
 (
 CUpti_ActivityKind kind
 )
);


CUPTI_FN
(
 cuptiActivityEnableContext,
 (
  CUcontext context,
  CUpti_ActivityKind kind
 )
);


CUPTI_FN
(
 cuptiActivityDisableContext,
 (
  CUcontext context,
  CUpti_ActivityKind kind
 )
);


CUPTI_FN
(
 cuptiActivityConfigurePCSampling,
 (
  CUcontext ctx,
  CUpti_ActivityPCSamplingConfig *config
 )
);


CUPTI_FN
(
 cuptiActivityRegisterCallbacks,
 (
  CUpti_BuffersCallbackRequestFunc funcBufferRequested,
  CUpti_BuffersCallbackCompleteFunc funcBufferCompleted
 )
);


CUPTI_FN
(
 cuptiActivityPushExternalCorrelationId,
 (
  CUpti_ExternalCorrelationKind kind,
  uint64_t id
 )
);


CUPTI_FN
(
 cuptiActivityPopExternalCorrelationId,
 (
  CUpti_ExternalCorrelationKind kind,
  uint64_t *lastId
 )
);


CUPTI_FN
(
 cuptiActivityGetNextRecord,
 (
  uint8_t* buffer,
  size_t validBufferSizeBytes,
  CUpti_Activity **record
 )
);


CUPTI_FN
(
 cuptiActivityGetNumDroppedRecords,
 (
  CUcontext context,
  uint32_t streamId,
  size_t *dropped
 )
);


CUPTI_FN
(
  cuptiActivitySetAttribute,
  (
   CUpti_ActivityAttribute attribute,
   size_t *value_size,
   void *value
  )
);


CUPTI_FN
(
 cuptiActivityFlushAll,
 (
  uint32_t flag
 )
);


CUPTI_FN
(
 cuptiGetTimestamp,
 (
  uint64_t *timestamp
 )
);


CUPTI_FN
(
  cuptiGetTimestamp,
  (
    uint64_t* timestamp
  )
);


CUPTI_FN
(
 cuptiEnableDomain,
 (
  uint32_t enable,
  CUpti_SubscriberHandle subscriber,
  CUpti_CallbackDomain domain
 )
);


CUPTI_FN
(
 cuptiFinalize,
 (
  void
 )
);


CUPTI_FN
(
 cuptiGetResultString,
 (
  CUptiResult result,
  const char **str
 )
);


CUPTI_FN
(
 cuptiSubscribe,
 (
  CUpti_SubscriberHandle *subscriber,
  CUpti_CallbackFunc callback,
  void *userdata
 )
);


CUPTI_FN
(
 cuptiUnsubscribe,
 (
  CUpti_SubscriberHandle subscriber
 )
);



//******************************************************************************
// private operations
//******************************************************************************

#ifndef HPCRUN_STATIC_LINK
int
cuda_path
(
 struct dl_phdr_info *info,
 size_t size,
 void *data
)
{
  char *buffer = (char *) data;
  const char *suffix = strstr(info->dlpi_name, "libcudart");
  if (suffix) {
    // CUDA library organization after 9.0
    suffix = strstr(info->dlpi_name, "targets");
    if (!suffix) {
      // CUDA library organization in 9.0 or earlier
      suffix = strstr(info->dlpi_name, "lib64");
    }
  }
  if (suffix){
    int len = suffix - info->dlpi_name;
    strncpy(buffer, info->dlpi_name, len);
    buffer[len] = 0;
    return 1;
  }
  return 0;
}


static void
cupti_set_default_path(char *buffer)
{
  strcpy(buffer, CUPTI_INSTALL_PREFIX CUPTI_LIBRARY_LOCATION);
}

int
library_path_resolves(const char *buffer)
{
  struct stat sb;
  return stat(buffer, &sb) == 0;
}


static const char *
cupti_path
(
  void
)
{
  const char *path = "libcupti.so";
  int resolved = 0;

  static char buffer[PATH_MAX];
  buffer[0] = 0;

  // open an NVIDIA library to find the CUDA path with dl_iterate_phdr
  // note: a version of this file with a more specific name may
  // already be loaded. thus, even if the dlopen fails, we search with
  // dl_iterate_phdr.
  void *h = monitor_real_dlopen("libcudart.so", RTLD_LOCAL | RTLD_LAZY);

  if (dl_iterate_phdr(cuda_path, buffer)) {
    // invariant: buffer contains CUDA home
    int zero_index = strlen(buffer);
    strcat(buffer, CUPTI_LIBRARY_LOCATION);

    if (library_path_resolves(buffer)) {
      path = buffer;
      resolved = 1;
    } else {
      buffer[zero_index] = 0;
      strcat(buffer, CUPTI_PATH_FROM_CUDA CUPTI_LIBRARY_LOCATION);

      if (library_path_resolves(buffer)) {
        path = buffer;
        resolved = 1;
      } else {
        buffer[zero_index - 1] = 0;
        fprintf(stderr, "NOTE: CUDA root at %s lacks a copy of NVIDIA's CUPTI "
          "tools library.\n", buffer);
      }
    }
  }

  if (!resolved) {
    cupti_set_default_path(buffer);
    if (library_path_resolves(buffer)) {
      fprintf(stderr, "NOTE: Using builtin path for NVIDIA's CUPTI tools "
        "library %s.\n", buffer);
      path = buffer;
      resolved = 1;
    }
  }

  if (h) monitor_real_dlclose(h);

  return path;
}

#endif

int
cupti_bind
(
  void
)
{
#ifndef HPCRUN_STATIC_LINK
  // dynamic libraries only availabile in non-static case
  hpcrun_force_dlopen(true);
  CHK_DLOPEN(cupti, cupti_path(), RTLD_NOW | RTLD_GLOBAL);
  hpcrun_force_dlopen(false);

#define CUPTI_BIND(fn) \
  CHK_DLSYM(cupti, fn);

  FORALL_CUPTI_ROUTINES(CUPTI_BIND);

#undef CUPTI_BIND

  return DYNAMIC_BINDING_STATUS_OK;
#else
  return DYNAMIC_BINDING_STATUS_ERROR;
#endif // ! HPCRUN_STATIC_LINK
}


static cct_node_t *
cupti_correlation_callback_dummy // __attribute__((unused))
(
 uint64_t id
)
{
  return NULL;
}


static void
cupti_error_callback_dummy // __attribute__((unused))
(
 const char *type,
 const char *fn,
 const char *error_string
)
{
  
  EEMSG("FATAL: hpcrun failure: failure type = %s, "
      "function %s failed with error %s", type, fn, error_string);
  EEMSG("See the 'FAQ and Troubleshooting' chapter in the HPCToolkit manual for guidance");
  exit(1);
}


static void
cupti_error_report
(
 CUptiResult error,
 const char *fn
)
{
  const char *error_string;
  CUPTI_FN_NAME(cuptiGetResultString)(error, &error_string);
  cupti_error_callback("CUPTI result error", fn, error_string);
}


//******************************************************************************
// private operations
//******************************************************************************

static bool
cupti_write_cubin
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


void
cupti_load_callback_cuda
(
 uint32_t cubin_id,
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
  used += sprintf(&file_name[used], "%s", "/" GPU_BINARY_DIRECTORY "/");
  mkdir(file_name, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  for (i = 0; i < hash_len; ++i) {
    used += sprintf(&file_name[used], "%02x", hash[i]);
  }
  used += sprintf(&file_name[used], "%s", GPU_BINARY_SUFFIX);
  TMSG(CUPTI, "cubin_id %d hash %s", cubin_id, file_name);

  // Write a file if does not exist
  bool file_flag;
  spinlock_lock(&files_lock);
  file_flag = cupti_write_cubin(file_name, cubin, cubin_size);
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
    TMSG(CUPTI, "cubin_id %d -> hpctoolkit_module_id %d", cubin_id, hpctoolkit_module_id);
    cubin_id_map_entry_t *entry = cubin_id_map_lookup(cubin_id);
    if (entry == NULL) {
      Elf_SymbolVector *vector = computeCubinFunctionOffsets(cubin, cubin_size);
      cubin_id_map_insert(cubin_id, hpctoolkit_module_id, vector);
    }
  }
}


void
cupti_unload_callback_cuda
(
 uint32_t cubin_id,
 const void *cubin,
 size_t cubin_size
)
{
  //cubin_id_map_delete(cubin_id);
}


static ip_normalized_t
cupti_func_ip_resolve
(
 CUfunction function
)
{
  hpctoolkit_cufunc_st_t *cufunc = (hpctoolkit_cufunc_st_t *)(function);
  hpctoolkit_cumod_st_t *cumod = (hpctoolkit_cumod_st_t *)cufunc->cumod;
  uint32_t function_index = cufunc->function_index;
  uint32_t cubin_id = cumod->cubin_id;
  ip_normalized_t ip_norm = cubin_id_transform(cubin_id, function_index, 0);
  TMSG(CUPTI_TRACE, "Decode function_index %u cubin_id %u", function_index, cubin_id);
  return ip_norm;
}


void
ensure_kernel_ip_present
(
 cct_node_t *kernel_ph, 
 ip_normalized_t kernel_ip
)
{
  // if the phaceholder was previously inserted, it will have a child
  // we only want to insert a child if there isn't one already. if the
  // node contains a child already, then the gpu monitoring thread 
  // may be adding children to the splay tree of children. in that case 
  // trying to add a child here (which will turn into a lookup of the
  // previously added child, would race with any insertions by the 
  // GPU monitoring thread.
  //
  // INVARIANT: avoid a race modifying the splay tree of children by 
  // not attempting to insert a child in a worker thread when a child 
  // is already present
  if (hpcrun_cct_children(kernel_ph) == NULL) {
    cct_node_t *kernel = 
      hpcrun_cct_insert_ip_norm(kernel_ph, kernel_ip);
    hpcrun_cct_retain(kernel);
  }
}


static void
cupti_subscriber_callback
(
 void *userdata,
 CUpti_CallbackDomain domain,
 CUpti_CallbackId cb_id,
 const void *cb_info
)
{
  if (domain == CUPTI_CB_DOMAIN_RESOURCE) {
    const CUpti_ResourceData *rd = (const CUpti_ResourceData *) cb_info;
    if (cb_id == CUPTI_CBID_RESOURCE_MODULE_LOADED) {
      CUpti_ModuleResourceData *mrd = (CUpti_ModuleResourceData *)
        rd->resourceDescriptor;

      TMSG(CUPTI, "loaded module id %d, cubin size %" PRIu64 ", cubin %p",
        mrd->moduleId, mrd->cubinSize, mrd->pCubin);
      DISPATCH_CALLBACK(cupti_load_callback, (mrd->moduleId, mrd->pCubin, mrd->cubinSize));
    } else if (cb_id == CUPTI_CBID_RESOURCE_MODULE_UNLOAD_STARTING) {
      CUpti_ModuleResourceData *mrd = (CUpti_ModuleResourceData *)
        rd->resourceDescriptor;

      TMSG(CUPTI, "unloaded module id %d, cubin size %" PRIu64 ", cubin %p",
        mrd->moduleId, mrd->cubinSize, mrd->pCubin);
      DISPATCH_CALLBACK(cupti_unload_callback, (mrd->moduleId, mrd->pCubin, mrd->cubinSize));
    } else if (cb_id == CUPTI_CBID_RESOURCE_CONTEXT_CREATED) {
      int pc_sampling_frequency = cupti_pc_sampling_frequency_get();
      if (pc_sampling_frequency != -1) {
        cupti_pc_sampling_enable(rd->context, pc_sampling_frequency);
      }
    }
  } else if (domain == CUPTI_CB_DOMAIN_DRIVER_API) {
    // stop flag is only set if a driver or runtime api called
    cupti_stop_flag_set();

    const CUpti_CallbackData *cd = (const CUpti_CallbackData *) cb_info;

    bool ompt_runtime_api_flag = ompt_runtime_status_get();

    bool is_valid_op = false;
    gpu_op_placeholder_flags_t gpu_op_placeholder_flags = 0;
    ip_normalized_t kernel_ip;

    switch (cb_id) {
      //FIXME(Keren): do not support memory allocate and free for current CUPTI version
      // FIXME(Dejan): here find #bytes from func argument list and atribute it to node in cct(corr_id)
      //case CUPTI_DRIVER_TRACE_CBID_cuMemAlloc:
      //case CUPTI_DRIVER_TRACE_CBID_cu64MemAlloc:
      //case CUPTI_DRIVER_TRACE_CBID_cuMemAllocPitch:
      //case CUPTI_DRIVER_TRACE_CBID_cu64MemAllocPitch:
      //case CUPTI_DRIVER_TRACE_CBID_cuMemAlloc_v2:
      //case CUPTI_DRIVER_TRACE_CBID_cuMemAllocPitch_v2:
      //  {
      //    cuda_state = cuda_placeholders.cuda_memalloc_state;
      //    is_valid_op = true;
      //    break;
      //  }
      // synchronize apis
      case CUPTI_DRIVER_TRACE_CBID_cuCtxSynchronize:
      case CUPTI_DRIVER_TRACE_CBID_cuEventSynchronize:
      case CUPTI_DRIVER_TRACE_CBID_cuStreamSynchronize:
      case CUPTI_DRIVER_TRACE_CBID_cuStreamSynchronize_ptsz:
      case CUPTI_DRIVER_TRACE_CBID_cuStreamWaitEvent:
      case CUPTI_DRIVER_TRACE_CBID_cuStreamWaitEvent_ptsz:
        {
          gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags,
            gpu_placeholder_type_sync);
          is_valid_op = true;
          break;
        }
      // copyin apis
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoD:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoDAsync:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoD_v2:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoDAsync_v2:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoD_v2_ptds:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoDAsync_v2_ptsz:
        {
          gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags,
            gpu_placeholder_type_copyin);
          is_valid_op = true;
          break;
        }
      // copyout apis
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoH:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoHAsync:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoH_v2:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoHAsync_v2:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoH_v2_ptds:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoHAsync_v2_ptsz:
        {
          gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags,
            gpu_placeholder_type_copyout);
          is_valid_op = true;
          break;
        }
      // copy apis
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoD:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoA:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoD:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoA:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoH:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoA:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy2D:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy2DUnaligned:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy3D:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoDAsync:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoAAsync:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoHAsync:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy2DAsync:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy3DAsync:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy_v2:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoD_v2:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoDAsync_v2:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoH_v2:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoHAsync_v2:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoD_v2:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoA_v2:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoA_v2:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy2D_v2:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy2DUnaligned_v2:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy2DAsync_v2:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy3D_v2:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy3DAsync_v2:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoA_v2:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoAAsync_v2:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyAsync:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyPeer:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyPeerAsync:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy3DPeer:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy3DPeerAsync:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoD_v2_ptds:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoA_v2_ptds:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoD_v2_ptds:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoA_v2_ptds:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoH_v2_ptds:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoA_v2_ptds:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy2D_v2_ptds:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy2DUnaligned_v2_ptds:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy3D_v2_ptds:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy_ptds:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyPeer_ptds:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy3DPeer_ptds:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyAsync_ptsz:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoAAsync_v2_ptsz:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoHAsync_v2_ptsz:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoDAsync_v2_ptsz:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy2DAsync_v2_ptsz:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy3DAsync_v2_ptsz:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyPeerAsync_ptsz:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy3DPeerAsync_ptsz:
        {
          gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags,
            gpu_placeholder_type_copy);
          is_valid_op = true;
          break;
        }
        // kernel apis
      case CUPTI_DRIVER_TRACE_CBID_cuLaunch:
      case CUPTI_DRIVER_TRACE_CBID_cuLaunchGrid:
      case CUPTI_DRIVER_TRACE_CBID_cuLaunchGridAsync:
      case CUPTI_DRIVER_TRACE_CBID_cuLaunchKernel:
      case CUPTI_DRIVER_TRACE_CBID_cuLaunchKernel_ptsz:
      case CUPTI_DRIVER_TRACE_CBID_cuLaunchCooperativeKernel:
      case CUPTI_DRIVER_TRACE_CBID_cuLaunchCooperativeKernel_ptsz:
      case CUPTI_DRIVER_TRACE_CBID_cuLaunchCooperativeKernelMultiDevice:
        {
          gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags,
            gpu_placeholder_type_kernel);

          gpu_op_placeholder_flags_set(&gpu_op_placeholder_flags,
            gpu_placeholder_type_trace);

          is_valid_op = true;

          if (cd->callbackSite == CUPTI_API_ENTER) {
            gpu_application_thread_process_activities();

            // XXX(Keren): cannot parse this kind of kernel launch
            //if (cb_id != CUPTI_DRIVER_TRACE_CBID_cuLaunchCooperativeKernelMultiDevice)
            // CUfunction is the first param
            CUfunction function_ptr = *(CUfunction *)((CUfunction)cd->functionParams);
            kernel_ip = cupti_func_ip_resolve(function_ptr);
          }
          break;
        }
      default:
        break;
    }
    bool is_kernel_op = gpu_op_placeholder_flags_is_set(gpu_op_placeholder_flags,
      gpu_placeholder_type_kernel);
    // If we have a valid operation and is not in the interval of a cuda/ompt runtime api
    if (is_valid_op && !cupti_runtime_api_flag && !ompt_runtime_api_flag) {
      if (cd->callbackSite == CUPTI_API_ENTER) {
        // A driver API cannot be implemented by other driver APIs, so we get an id
        // and unwind when the API is entered

        uint64_t correlation_id = gpu_correlation_id();
        cupti_correlation_id_push(correlation_id);

        cct_node_t *api_node = cupti_correlation_callback(correlation_id);

        gpu_op_ccts_t gpu_op_ccts;

        hpcrun_safe_enter();

        gpu_op_ccts_insert(api_node, &gpu_op_ccts, gpu_op_placeholder_flags);

        if (is_kernel_op) {
          cct_node_t *kernel_ph = gpu_op_ccts_get(&gpu_op_ccts, gpu_placeholder_type_kernel);

	  ensure_kernel_ip_present(kernel_ph, kernel_ip);

          cct_node_t *trace_ph = 
	    gpu_op_ccts_get(&gpu_op_ccts, gpu_placeholder_type_trace);

	  ensure_kernel_ip_present(trace_ph, kernel_ip);
        }

        hpcrun_safe_exit();

        // Generate notification entry
        uint64_t cpu_submit_time = hpcrun_nanotime();
        gpu_correlation_channel_produce(correlation_id, &gpu_op_ccts,
          cpu_submit_time);

        TMSG(CUPTI_TRACE, "Driver push externalId %lu (cb_id = %u)", correlation_id, cb_id);
      } else if (cd->callbackSite == CUPTI_API_EXIT) {
        uint64_t correlation_id __attribute__((unused)); // not used if PRINT omitted
        correlation_id = cupti_correlation_id_pop();
        TMSG(CUPTI_TRACE, "Driver pop externalId %lu (cb_id = %u)", correlation_id, cb_id);
      }
    } else if (is_kernel_op && cupti_runtime_api_flag && cd->callbackSite ==
      CUPTI_API_ENTER) {
      if (cupti_kernel_ph != NULL) {
        ensure_kernel_ip_present(cupti_kernel_ph, kernel_ip);
      }
      if (cupti_trace_ph != NULL) {
	ensure_kernel_ip_present(cupti_trace_ph, kernel_ip);
      }
    } else if (is_kernel_op && ompt_runtime_api_flag && cd->callbackSite ==
      CUPTI_API_ENTER) {
      cct_node_t *ompt_trace_node = ompt_trace_node_get();
      if (ompt_trace_node != NULL) {
        ensure_kernel_ip_present(ompt_trace_node, kernel_ip);
      }
    }
  } else if (domain == CUPTI_CB_DOMAIN_RUNTIME_API) {
    // stop flag is only set if a driver or runtime api called
    cupti_stop_flag_set();

    const CUpti_CallbackData *cd = (const CUpti_CallbackData *)cb_info;

    bool is_valid_op = false;
    bool is_kernel_op __attribute__((unused)) = false; // used only by PRINT when debugging
    switch (cb_id) {
      // FIXME(Keren): do not support memory allocate and free for
      // current CUPTI version
      //case CUPTI_RUNTIME_TRACE_CBID_cudaMalloc_v3020:
      //case CUPTI_RUNTIME_TRACE_CBID_cudaMallocPitch_v3020:
      //case CUPTI_RUNTIME_TRACE_CBID_cudaMallocArray_v3020:
      //case CUPTI_RUNTIME_TRACE_CBID_cudaMalloc3D_v3020:
      //case CUPTI_RUNTIME_TRACE_CBID_cudaMalloc3DArray_v3020:
      //  {
      //    cuda_state = cuda_placeholders.cuda_memalloc_state;
      //    is_valid_op = true;
      //    break;
      //  }
      // cuda synchronize apis
      case CUPTI_RUNTIME_TRACE_CBID_cudaEventSynchronize_v3020:
      case CUPTI_RUNTIME_TRACE_CBID_cudaStreamSynchronize_v3020:
      case CUPTI_RUNTIME_TRACE_CBID_cudaStreamSynchronize_ptsz_v7000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaStreamWaitEvent_v3020:
      case CUPTI_RUNTIME_TRACE_CBID_cudaDeviceSynchronize_v3020:
      // cuda copy apis
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyPeer_v4000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyPeerAsync_v4000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy3DPeer_v4000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy3DPeerAsync_v4000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy3D_v3020:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy3DAsync_v3020:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy3D_ptds_v7000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy3DAsync_ptsz_v7000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy3DPeer_ptds_v7000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy3DPeerAsync_ptsz_v7000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy_v3020:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2D_v3020:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyToArray_v3020:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DToArray_v3020:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyFromArray_v3020:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DFromArray_v3020:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyArrayToArray_v3020:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DArrayToArray_v3020:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyToSymbol_v3020:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyFromSymbol_v3020:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyAsync_v3020:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyToArrayAsync_v3020:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyFromArrayAsync_v3020:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DAsync_v3020:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DToArrayAsync_v3020:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DFromArrayAsync_v3020:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyToSymbolAsync_v3020:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyFromSymbolAsync_v3020:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy_ptds_v7000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2D_ptds_v7000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyToArray_ptds_v7000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DToArray_ptds_v7000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyFromArray_ptds_v7000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DFromArray_ptds_v7000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyArrayToArray_ptds_v7000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DArrayToArray_ptds_v7000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyToSymbol_ptds_v7000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyFromSymbol_ptds_v7000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyAsync_ptsz_v7000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyToArrayAsync_ptsz_v7000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyFromArrayAsync_ptsz_v7000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DAsync_ptsz_v7000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DToArrayAsync_ptsz_v7000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DFromArrayAsync_ptsz_v7000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyToSymbolAsync_ptsz_v7000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyFromSymbolAsync_ptsz_v7000:
        {
          is_valid_op = true;
          break;
        }
      // cuda kernel apis
      case CUPTI_RUNTIME_TRACE_CBID_cudaLaunch_v3020:
      case CUPTI_RUNTIME_TRACE_CBID_cudaLaunchKernel_v7000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaLaunch_ptsz_v7000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaLaunchKernel_ptsz_v7000:
      #if CUPTI_API_VERSION >= 10
      case CUPTI_RUNTIME_TRACE_CBID_cudaLaunchCooperativeKernel_v9000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaLaunchCooperativeKernel_ptsz_v9000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaLaunchCooperativeKernelMultiDevice_v9000:
      #endif
        {
          is_valid_op = true;
          is_kernel_op = true;
          if (cd->callbackSite == CUPTI_API_ENTER) {
            gpu_application_thread_process_activities();
          }
          break;
        }
      default:
        break;
    }
    if (is_valid_op) {
      if (cd->callbackSite == CUPTI_API_ENTER) {
        // Enter a CUDA runtime api
        cupti_runtime_api_flag_set();
        uint64_t correlation_id = gpu_correlation_id();
        cupti_correlation_id_push(correlation_id);
        // We should make notification records in the api enter callback.
        // A runtime API must be implemented by driver APIs.
        // Though unlikely in most cases,
        // it is still possible that a cupti buffer is full and returned to the host
        // in the interval of a runtime api.
        cct_node_t *api_node = cupti_correlation_callback(correlation_id);

        gpu_op_ccts_t gpu_op_ccts;

        hpcrun_safe_enter();

        gpu_op_ccts_insert(api_node, &gpu_op_ccts, gpu_op_placeholder_flags_all);

        hpcrun_safe_exit();

        cupti_kernel_ph = gpu_op_ccts_get(&gpu_op_ccts, gpu_placeholder_type_kernel);
        cupti_trace_ph = gpu_op_ccts_get(&gpu_op_ccts, gpu_placeholder_type_trace);

        // Generate notification entry
        uint64_t cpu_submit_time = hpcrun_nanotime();
        gpu_correlation_channel_produce(correlation_id, &gpu_op_ccts,
          cpu_submit_time);

        TMSG(CUPTI_TRACE, "Runtime push externalId %lu (cb_id = %u)", correlation_id, cb_id);
      } else if (cd->callbackSite == CUPTI_API_EXIT) {
        // Exit an CUDA runtime api
        cupti_runtime_api_flag_unset();

        uint64_t correlation_id __attribute__((unused)); // not used if PRINT omitted
        correlation_id = cupti_correlation_id_pop();
        TMSG(CUPTI_TRACE, "Runtime pop externalId %lu (cb_id = %u)", correlation_id, cb_id);

        cupti_kernel_ph = NULL;
        cupti_trace_ph = NULL;
      }
    } else {
      TMSG(CUPTI_TRACE, "Go through runtime with kernel_op %d, valid_op %d, "
        "cuda_runtime %d", is_kernel_op, is_valid_op, cupti_runtime_api_flag);
    }
  }
}



//******************************************************************************
// interface  operations
//******************************************************************************

void
cupti_device_timestamp_get
(
 CUcontext context,
 uint64_t *time
)
{
  HPCRUN_CUPTI_CALL(cuptiGetTimestamp, (time));
}


void
cupti_activity_timestamp_get
(
 uint64_t *time
)
{
  HPCRUN_CUPTI_CALL(cuptiGetTimestamp, (time));
}


void
cupti_device_buffer_config
(
 size_t buf_size,
 size_t sem_size
)
{
  size_t value_size = sizeof(size_t);
  HPCRUN_CUPTI_CALL(cuptiActivitySetAttribute,
                   (CUPTI_ACTIVITY_ATTR_DEVICE_BUFFER_SIZE, &value_size, &buf_size));
  HPCRUN_CUPTI_CALL(cuptiActivitySetAttribute,
                   (CUPTI_ACTIVITY_ATTR_PROFILING_SEMAPHORE_POOL_SIZE, &value_size, &sem_size));
}


void
cupti_buffer_alloc
(
 uint8_t **buffer,
 size_t *buffer_size,
 size_t *maxNumRecords
)
{
  // cupti client call this function
  int retval = posix_memalign((void **) buffer,
    (size_t) HPCRUN_CUPTI_ACTIVITY_BUFFER_ALIGNMENT,
    (size_t) HPCRUN_CUPTI_ACTIVITY_BUFFER_SIZE);

  if (retval != 0) {
    cupti_error_callback("CUPTI", "cupti_buffer_alloc", "out of memory");
  }

  *buffer_size = HPCRUN_CUPTI_ACTIVITY_BUFFER_SIZE;

  *maxNumRecords = 0;
}


bool
cupti_buffer_cursor_advance
(
  uint8_t *buffer,
  size_t size,
  CUpti_Activity **current
)
{
  return (CUPTI_FN_NAME(cuptiActivityGetNextRecord)(buffer, size, current) == CUPTI_SUCCESS);
}


void
cupti_buffer_completion_callback
(
 CUcontext ctx,
 uint32_t streamId,
 uint8_t *buffer,
 size_t size,
 size_t validSize
)
{
  
  hpcrun_thread_init_mem_pool_once();

  // handle notifications
  cupti_buffer_completion_notify();

  if (validSize > 0) {
    // Signal advance to return pointer to first record
    CUpti_Activity *cupti_activity = NULL;
    bool status = false;
    size_t processed = 0;
    do {
      status = cupti_buffer_cursor_advance(buffer, validSize, &cupti_activity);
      if (status) {
        cupti_activity_process(cupti_activity);
        ++processed;
      }
    } while (status);
    hpcrun_stats_acc_trace_records_add(processed);

    size_t dropped;
    cupti_num_dropped_records_get(ctx, streamId, &dropped);
    if (dropped != 0) {
      hpcrun_stats_acc_trace_records_dropped_add(dropped);
    }
  }

  free(buffer);
}


//-------------------------------------------------------------
// event specification
//-------------------------------------------------------------

cupti_set_status_t
cupti_monitoring_set
(
 const CUpti_ActivityKind activity_kinds[],
 bool enable
)
{
  TMSG(CUPTI, "enter cupti_set_monitoring");
  int failed = 0;
  int succeeded = 0;

  cupti_activity_enable_t action =
    (enable ?
     CUPTI_FN_NAME(cuptiActivityEnable):
     CUPTI_FN_NAME(cuptiActivityDisable));

  int i = 0;
  for (;;) {
    CUpti_ActivityKind activity_kind = activity_kinds[i++];
    if (activity_kind == CUPTI_ACTIVITY_KIND_INVALID) break;
    bool succ = action(activity_kind) == CUPTI_SUCCESS;
    if (succ) {
      if (enable) {
        TMSG(CUPTI, "activity %d enable succeeded", activity_kind);
      } else {
        TMSG(CUPTI, "activity %d disable succeeded", activity_kind);
      }
      succeeded++;
    } else {
      if (enable) {
        TMSG(CUPTI, "activity %d enable failed", activity_kind);
      } else {
        TMSG(CUPTI, "activity %d disable failed", activity_kind);
      }
      failed++;
    }
  }
  if (succeeded > 0) {
    if (failed == 0) return cupti_set_all;
    else return cupti_set_some;
  }
  TMSG(CUPTI, "leave cupti_set_monitoring");
  return cupti_set_none;
}


//-------------------------------------------------------------
// control apis
//-------------------------------------------------------------

void
cupti_init
(
 void
)
{
  cupti_activity_enabled.buffer_request = cupti_buffer_alloc;
  cupti_activity_enabled.buffer_complete = cupti_buffer_completion_callback;
}


void
cupti_start
(
 void
)
{
  HPCRUN_CUPTI_CALL(cuptiActivityRegisterCallbacks,
                   (cupti_activity_enabled.buffer_request,
                    cupti_activity_enabled.buffer_complete));
}


void
cupti_finalize
(
 void
)
{
  HPCRUN_CUPTI_CALL(cuptiFinalize, ());
}


void
cupti_num_dropped_records_get
(
 CUcontext context,
 uint32_t streamId,
 size_t* dropped
)
{
  HPCRUN_CUPTI_CALL(cuptiActivityGetNumDroppedRecords,
                   (context, streamId, dropped));
}


//-------------------------------------------------------------
// correlation callback control
//-------------------------------------------------------------

void
cupti_callbacks_subscribe
(
 void
)
{
  cupti_load_callback = cupti_load_callback_cuda;
  cupti_unload_callback = cupti_unload_callback_cuda;
  cupti_correlation_callback = gpu_application_thread_correlation_callback;

  HPCRUN_CUPTI_CALL(cuptiSubscribe, (&cupti_subscriber,
                   (CUpti_CallbackFunc) cupti_subscriber_callback,
                   (void *) NULL));

  HPCRUN_CUPTI_CALL(cuptiEnableDomain,
                   (1, cupti_subscriber, CUPTI_CB_DOMAIN_DRIVER_API));

  HPCRUN_CUPTI_CALL(cuptiEnableDomain,
                   (1, cupti_subscriber, CUPTI_CB_DOMAIN_RUNTIME_API));

  HPCRUN_CUPTI_CALL(cuptiEnableDomain,
                   (1, cupti_subscriber, CUPTI_CB_DOMAIN_RESOURCE));
}


void
cupti_callbacks_unsubscribe
(
)
{
  cupti_load_callback = 0;
  cupti_unload_callback = 0;
  cupti_correlation_callback = 0;

  HPCRUN_CUPTI_CALL(cuptiUnsubscribe, (cupti_subscriber));

  HPCRUN_CUPTI_CALL(cuptiEnableDomain,
                   (0, cupti_subscriber, CUPTI_CB_DOMAIN_DRIVER_API));

  HPCRUN_CUPTI_CALL(cuptiEnableDomain,
                   (0, cupti_subscriber, CUPTI_CB_DOMAIN_RUNTIME_API));

  HPCRUN_CUPTI_CALL(cuptiEnableDomain,
                   (0, cupti_subscriber, CUPTI_CB_DOMAIN_RESOURCE));
}


void
cupti_correlation_enable
(
)
{
  TMSG(CUPTI, "enter cupti_correlation_enable");
  cupti_correlation_enabled = true;

  // For unknown reasons, external correlation ids do not return using
  // cuptiActivityEnableContext
  HPCRUN_CUPTI_CALL(cuptiActivityEnable,
                   (CUPTI_ACTIVITY_KIND_EXTERNAL_CORRELATION));

  TMSG(CUPTI, "exit cupti_correlation_enable");
}


void
cupti_correlation_disable
(
)
{
  if (cupti_correlation_enabled) {
    HPCRUN_CUPTI_CALL(cuptiActivityDisable,
                     (CUPTI_ACTIVITY_KIND_EXTERNAL_CORRELATION));
    cupti_correlation_enabled = false;
  }
}


void
cupti_pc_sampling_enable
(
 CUcontext context,
 int frequency
)
{
  TMSG(CUPTI, "enter cupti_pc_sampling_enable");
  cupti_pc_sampling_enabled = true;
  CUpti_ActivityPCSamplingConfig config;
  config.samplingPeriod = 0;
  config.samplingPeriod2 = frequency;
  config.size = sizeof(config);

  int required;
  int retval = cuda_global_pc_sampling_required(&required);

  if (retval == 0) { // only turn something on if success determining mode

    if (!required) {
      HPCRUN_CUPTI_CALL(cuptiActivityConfigurePCSampling, (context, &config));

      HPCRUN_CUPTI_CALL(cuptiActivityEnableContext,
                        (context, CUPTI_ACTIVITY_KIND_PC_SAMPLING));
     } else {
      HPCRUN_CUPTI_CALL(cuptiActivityEnable, (CUPTI_ACTIVITY_KIND_PC_SAMPLING));
     }
  }

  TMSG(CUPTI, "exit cupti_pc_sampling_enable");
}


void
cupti_pc_sampling_disable
(
 CUcontext context
)
{
  if (cupti_pc_sampling_enabled) {
    HPCRUN_CUPTI_CALL(cuptiActivityDisableContext,
                     (context, CUPTI_ACTIVITY_KIND_PC_SAMPLING));

    cupti_pc_sampling_enabled = false;
  }
}



//******************************************************************************
// finalizer
//******************************************************************************

void
cupti_activity_flush
(
)
{
  if (cupti_stop_flag) {
    cupti_stop_flag_unset();
    HPCRUN_CUPTI_CALL(cuptiActivityFlushAll, (CUPTI_ACTIVITY_FLAG_FLUSH_FORCED));
  }
}


void
cupti_device_flush(void *args, int how)
{
  cupti_activity_flush();
  // TODO(keren): replace cupti with sth. called device queue
  gpu_application_thread_process_activities();
}


void
cupti_stop_flag_set()
{
  cupti_stop_flag = true;
}


void
cupti_stop_flag_unset()
{
  cupti_stop_flag = false;
}


void
cupti_runtime_api_flag_unset()
{
  cupti_runtime_api_flag = false;
}


void
cupti_runtime_api_flag_set()
{
  cupti_runtime_api_flag = true;
}


void
cupti_correlation_id_push(uint64_t id)
{
  HPCRUN_CUPTI_CALL(cuptiActivityPushExternalCorrelationId,
    (CUPTI_EXTERNAL_CORRELATION_KIND_UNKNOWN, id));
}


uint64_t
cupti_correlation_id_pop()
{
  uint64_t id;
  HPCRUN_CUPTI_CALL(cuptiActivityPopExternalCorrelationId,
    (CUPTI_EXTERNAL_CORRELATION_KIND_UNKNOWN, &id));
  return id;
}


void
cupti_device_init()
{
  cupti_stop_flag = false;
  cupti_runtime_api_flag = false;

  // FIXME: Callback shutdown currently disabled to handle issues with fork()
  // See the comment preceeding sample-sources/nvidia.c:process_event_list for details.

  // cupti_correlation_enabled = false;
  // cupti_pc_sampling_enabled = false;

  // cupti_correlation_callback = cupti_correlation_callback_dummy;

  // cupti_error_callback = cupti_error_callback_dummy;

  // cupti_activity_enabled.buffer_request = 0;
  // cupti_activity_enabled.buffer_complete = 0;

  // cupti_load_callback = 0;

  // cupti_unload_callback = 0;
}


void
cupti_device_shutdown(void *args, int how)
{
  cupti_callbacks_unsubscribe();
  cupti_device_flush(args, how);
}

