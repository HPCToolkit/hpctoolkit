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
//   cupti-api.c
//
// Purpose:
//   implementation of wrapper around NVIDIA's CUPTI performance tools API
//  
//***************************************************************************

//***************************************************************************
// system includes
//***************************************************************************

#include <stdio.h>
#include <errno.h>     // errno
#include <fcntl.h>     // open
#include <limits.h>    // PATH_MAX
// #include <pthread.h>
#include <stdio.h>     // sprintf
#include <unistd.h>
#include <sys/stat.h>  // mkdir

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

#include "cupti-api.h"
#include "cupti-correlation-id-map.h"
#include "cupti-function-id-map.h"
#include "cupti-host-op-map.h"
#include "cupti-record.h"



//******************************************************************************
// macros
//******************************************************************************

#define CUPTI_API_DEBUG 0

#if CUPTI_API_DEBUG
#define PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define PRINT(...)
#endif

#define HPCRUN_CUPTI_ACTIVITY_BUFFER_SIZE (16 * 1024 * 1024)
#define HPCRUN_CUPTI_ACTIVITY_BUFFER_ALIGNMENT (8)

#define CUPTI_FN_NAME(f) DYN_FN_NAME(f)

#define CUPTI_FN(fn, args) \
  static CUptiResult (*CUPTI_FN_NAME(fn)) args

#define HPCRUN_CUPTI_CALL(fn, args) \
{      \
  CUptiResult status = CUPTI_FN_NAME(fn) args;	\
  if (status != CUPTI_SUCCESS) {		\
    cupti_error_report(status, #fn);		\
  }						\
}

#define DISPATCH_CALLBACK(fn, args) if (fn) fn args

#define FORALL_CUPTI_ROUTINES(macro)			\
  macro(cuptiActivityConfigurePCSampling)		\
  macro(cuptiActivityDisable)				\
  macro(cuptiActivityDisableContext)			\
  macro(cuptiActivityEnable)				\
  macro(cuptiActivityEnableContext)			\
  macro(cuptiActivityFlushAll)				\
  macro(cuptiActivitySetAttribute)				\
  macro(cuptiActivityGetNextRecord)			\
  macro(cuptiActivityGetNumDroppedRecords)		\
  macro(cuptiActivityPopExternalCorrelationId)		\
  macro(cuptiActivityPushExternalCorrelationId)		\
  macro(cuptiActivityRegisterCallbacks)			\
  macro(cuptiDeviceGetTimestamp)			\
  macro(cuptiEnableDomain)				\
  macro(cuptiFinalize)					\
  macro(cuptiGetResultString)				\
  macro(cuptiSubscribe)					\
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
 CUcontext context,
 CUpti_ActivityKind activity
);


typedef void (*cupti_correlation_callback_t)
(
 uint64_t *id,
 cuda_placeholder_t cuda_state
);


typedef void (*cupti_load_callback_t)
(
 int module_id, 
 const void *cubin, 
 size_t cubin_size
);


typedef struct {
  CUpti_BuffersCallbackRequestFunc buffer_request; 
  CUpti_BuffersCallbackCompleteFunc buffer_complete;
} cupti_activity_buffer_state_t;



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


static void 
cupti_correlation_callback_dummy
(
 uint64_t *id,
 cuda_placeholder_t cuda_state
);



//******************************************************************************
// static data
//******************************************************************************

static atomic_long cupti_correlation_id = ATOMIC_VAR_INIT(1);

static spinlock_t files_lock = SPINLOCK_UNLOCKED;

static __thread bool cupti_stop_flag = false;
static __thread bool cupti_enter_runtime_api = false;

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
 cuptiDeviceGetTimestamp,
 (
  CUcontext context,
  uint64_t *timestamp
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


const char *
cupti_path
(
  void
)
{
  const char *path = "libcupti.so";

  static char buffer[PATH_MAX];
  buffer[0] = 0;

  // open an NVIDIA library to find the CUDA path with dl_iterate_phdr
  // note: a version of this file with a more specific name may 
  // already be loaded. thus, even if the dlopen fails, we search with
  // dl_iterate_phdr.
  void *h = monitor_real_dlopen("libcudart.so", RTLD_LOCAL | RTLD_LAZY);

  if (dl_iterate_phdr(cuda_path, buffer)) {
    // invariant: buffer contains CUDA home 
    strcat(buffer, "extras/CUPTI/lib64/libcupti.so");
    path = buffer;
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

  FORALL_CUPTI_ROUTINES(CUPTI_BIND)

#undef CUPTI_BIND

  return 0;
#else
  return -1;
#endif // ! HPCRUN_STATIC_LINK
}


static void 
cupti_correlation_callback_dummy // __attribute__((unused))
(
 uint64_t *id,
 cuda_placeholder_t cuda_state
)
{
  *id = 0;
}


static void
cupti_error_callback_dummy // __attribute__((unused))
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
 int module_id, 
 const void *cubin, 
 size_t cubin_size
)
{
  // Compute hash for cubin and store it into a map
  cubin_hash_map_entry_t *entry = cubin_hash_map_lookup(module_id);
  unsigned char *hash;
  unsigned int hash_len;
  if (entry == NULL) {
    cubin_hash_map_insert(module_id, cubin, cubin_size);
    entry = cubin_hash_map_lookup(module_id);
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
  PRINT("module_id %d hash %s\n", module_id, file_name);

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
    PRINT("module_id %d -> hpctoolkit_module_id %d\n", module_id, 
	  hpctoolkit_module_id);
    cubin_id_map_entry_t *entry = cubin_id_map_lookup(module_id);
    if (entry == NULL) {
      Elf_SymbolVector *vector = computeCubinFunctionOffsets(cubin, cubin_size);
      cubin_id_map_insert(module_id, hpctoolkit_module_id, vector);
    }
  }
}


void
cupti_unload_callback_cuda
(
 int module_id, 
 const void *cubin, 
 size_t cubin_size
)
{
  //cubin_id_map_delete(module_id);
}


static void
cupti_correlation_callback_cuda
(
 uint64_t *id,
 cuda_placeholder_t cuda_state
)
{
  *id = atomic_fetch_add(&cupti_correlation_id, 1);
  
  PRINT("enter cupti_correlation_callback_cuda %u\n", *id);
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
  // Highest cupti node
  cct_addr_t *node_addr = hpcrun_cct_addr(node);
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
  cct_addr_t frm_api;
  memset(&frm_api, 0, sizeof(cct_addr_t));
  frm_api.ip_norm = cuda_state.pc_norm;
  cct_node_t* cct_api = hpcrun_cct_insert_addr(node, &frm_api);

  hpcrun_safe_exit();
  td->overhead--;

  // Generate notification entry
  cupti_worker_notification_apply(*id, cct_api);

  PRINT("exit cupti_correlation_callback_cuda\n");
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
      PRINT("loaded module id %d, cubin size %ld, cubin %p\n", 
        mrd->moduleId, mrd->cubinSize, mrd->pCubin);

      DISPATCH_CALLBACK(cupti_load_callback, (mrd->moduleId, mrd->pCubin, 
					      mrd->cubinSize));

    } else if (cb_id == CUPTI_CBID_RESOURCE_MODULE_UNLOAD_STARTING) {
      CUpti_ModuleResourceData *mrd = (CUpti_ModuleResourceData *) 
        rd->resourceDescriptor;
      PRINT("unloaded module id %d, cubin size %ld, cubin %p\n", 
        mrd->moduleId, mrd->cubinSize, mrd->pCubin);

      DISPATCH_CALLBACK(cupti_unload_callback, 
			(mrd->moduleId, mrd->pCubin, mrd->cubinSize));

    } else if (cb_id == CUPTI_CBID_RESOURCE_CONTEXT_CREATED) {
      cupti_enable_activities(rd->context);
    }
  } else if (domain == CUPTI_CB_DOMAIN_DRIVER_API) {
    if (cupti_enter_runtime_api) {
      return;
    }

    // stop flag is only set if a driver or runtime api called
    cupti_stop_flag_set();
    cupti_record_init();

    const CUpti_CallbackData *cd = (const CUpti_CallbackData *) cb_info;
    cuda_placeholder_t cuda_state = cuda_placeholders.cuda_none_state;

    bool is_valid_cuda_op = false;
    switch (cb_id) {
      case CUPTI_DRIVER_TRACE_CBID_cuCtxSynchronize:
      case CUPTI_DRIVER_TRACE_CBID_cuEventSynchronize:
      case CUPTI_DRIVER_TRACE_CBID_cuStreamSynchronize:
      case CUPTI_DRIVER_TRACE_CBID_cuStreamSynchronize_ptsz:
      case CUPTI_DRIVER_TRACE_CBID_cuStreamWaitEvent:
      case CUPTI_DRIVER_TRACE_CBID_cuStreamWaitEvent_ptsz:
        {
          cuda_state = cuda_placeholders.cuda_sync_state;
          is_valid_cuda_op = true;
          break;
        }
      //FIXME(Keren): do not support memory allocate and free for current CUPTI version
      //case CUPTI_DRIVER_TRACE_CBID_cuMemAlloc:
      //case CUPTI_DRIVER_TRACE_CBID_cu64MemAlloc:
      //case CUPTI_DRIVER_TRACE_CBID_cuMemAllocPitch:
      //case CUPTI_DRIVER_TRACE_CBID_cu64MemAllocPitch:
      //case CUPTI_DRIVER_TRACE_CBID_cuMemAlloc_v2:
      //case CUPTI_DRIVER_TRACE_CBID_cuMemAllocPitch_v2:
      //  {
      //    cuda_state = cuda_placeholders.cuda_memalloc_state;
      //    is_valid_cuda_op = true;
      //    break;
      //  }
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoD:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoH:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoD:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoA:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoD:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoA:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoH:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoA:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy2D:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy2DUnaligned:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy3D:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoDAsync:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoHAsync:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoDAsync:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoAAsync:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoHAsync:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy2DAsync:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy3DAsync:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy_v2:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoD_v2:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoDAsync_v2:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoH_v2:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoHAsync_v2:
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
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoD_v2_ptds:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoH_v2_ptds:
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
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoDAsync_v2_ptsz:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoHAsync_v2_ptsz:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoDAsync_v2_ptsz:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy2DAsync_v2_ptsz:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy3DAsync_v2_ptsz:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpyPeerAsync_ptsz:
      case CUPTI_DRIVER_TRACE_CBID_cuMemcpy3DPeerAsync_ptsz:
        {
          cuda_state = cuda_placeholders.cuda_memcpy_state;
          is_valid_cuda_op = true;
          break;
        }
      case CUPTI_DRIVER_TRACE_CBID_cuLaunch:
      case CUPTI_DRIVER_TRACE_CBID_cuLaunchGrid:
      case CUPTI_DRIVER_TRACE_CBID_cuLaunchGridAsync:
      case CUPTI_DRIVER_TRACE_CBID_cuLaunchKernel:
      case CUPTI_DRIVER_TRACE_CBID_cuLaunchKernel_ptsz:
      case CUPTI_DRIVER_TRACE_CBID_cuLaunchCooperativeKernel:
      case CUPTI_DRIVER_TRACE_CBID_cuLaunchCooperativeKernel_ptsz:
      case CUPTI_DRIVER_TRACE_CBID_cuLaunchCooperativeKernelMultiDevice:
        {
          // Process previous activities
          cupti_worker_activity_apply(cupti_activity_handle);
          cuda_state = cuda_placeholders.cuda_kernel_state;
          is_valid_cuda_op = true;
          break;
        }
      default:
        break;
    }
    if (is_valid_cuda_op) {
      if (cd->callbackSite == CUPTI_API_ENTER) {
        uint64_t correlation_id = 0;
        cupti_correlation_callback(&correlation_id, cuda_state);
        HPCRUN_CUPTI_CALL(cuptiActivityPushExternalCorrelationId,
          (CUPTI_EXTERNAL_CORRELATION_KIND_UNKNOWN, correlation_id));
        PRINT("Driver push externalId %lu (cb_id = %u)\n", correlation_id, 
          cb_id);
      }
      if (cd->callbackSite == CUPTI_API_EXIT) {
        uint64_t correlation_id;
        HPCRUN_CUPTI_CALL(cuptiActivityPopExternalCorrelationId,
          (CUPTI_EXTERNAL_CORRELATION_KIND_UNKNOWN, &correlation_id));
        PRINT("Driver pop externalId %lu (cb_id = %u)\n", correlation_id, 
          cb_id);
      }
    }
  } else if (domain == CUPTI_CB_DOMAIN_RUNTIME_API) { 
    // stop flag is only set if a driver or runtime api called
    cupti_stop_flag_set();
    cupti_record_init();

    const CUpti_CallbackData *cd = (const CUpti_CallbackData *) cb_info;
    cuda_placeholder_t cuda_state = cuda_placeholders.cuda_none_state;

    bool is_valid_cuda_op = false;
    switch (cb_id) {
      case CUPTI_RUNTIME_TRACE_CBID_cudaEventSynchronize_v3020:
      case CUPTI_RUNTIME_TRACE_CBID_cudaStreamSynchronize_v3020:
      case CUPTI_RUNTIME_TRACE_CBID_cudaStreamSynchronize_ptsz_v7000:
      case CUPTI_RUNTIME_TRACE_CBID_cudaStreamWaitEvent_v3020:
      case CUPTI_RUNTIME_TRACE_CBID_cudaDeviceSynchronize_v3020: 
        {
          cuda_state = cuda_placeholders.cuda_sync_state;
          is_valid_cuda_op = true;
          break;
        }
      // FIXME(Keren): do not support memory allocate and free for 
      // current CUPTI version
      //case CUPTI_RUNTIME_TRACE_CBID_cudaMalloc_v3020:
      //case CUPTI_RUNTIME_TRACE_CBID_cudaMallocPitch_v3020:
      //case CUPTI_RUNTIME_TRACE_CBID_cudaMallocArray_v3020:
      //case CUPTI_RUNTIME_TRACE_CBID_cudaMalloc3D_v3020:
      //case CUPTI_RUNTIME_TRACE_CBID_cudaMalloc3DArray_v3020:
      //  {
      //    cuda_state = cuda_placeholders.cuda_memalloc_state;
      //    is_valid_cuda_op = true;
      //    break;
      //  }
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
          cuda_state = cuda_placeholders.cuda_memcpy_state;
          is_valid_cuda_op = true;
          break;
        }
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
          // Process previous activities
          cupti_worker_activity_apply(cupti_activity_handle);
          cuda_state = cuda_placeholders.cuda_kernel_state;
          is_valid_cuda_op = true;
          break;
        }
      default:
        break;
    }
    if (is_valid_cuda_op) {
      if (cd->callbackSite == CUPTI_API_ENTER) {
        cupti_enter_runtime_api = true;
        uint64_t correlation_id = 0;
        cupti_correlation_callback(&correlation_id, cuda_state);
        PRINT("Runtime push externalId %lu (cb_id = %u)\n", correlation_id, 
          cb_id);
        HPCRUN_CUPTI_CALL(cuptiActivityPushExternalCorrelationId,
          (CUPTI_EXTERNAL_CORRELATION_KIND_UNKNOWN, correlation_id));
      }
      if (cd->callbackSite == CUPTI_API_EXIT) {
        cupti_enter_runtime_api = false;
        uint64_t correlation_id = 0;
        HPCRUN_CUPTI_CALL(cuptiActivityPopExternalCorrelationId,
          (CUPTI_EXTERNAL_CORRELATION_KIND_UNKNOWN, &correlation_id));
        PRINT("Runtime pop externalId %lu (cb_id = %u)\n", correlation_id, 
	      cb_id);
      }
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
  HPCRUN_CUPTI_CALL(cuptiDeviceGetTimestamp, (context, time));
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
  return (CUPTI_FN_NAME(cuptiActivityGetNextRecord)
	  (buffer, size, current) == CUPTI_SUCCESS);
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
  // handle notifications
  cupti_cupti_notification_apply(cupti_notification_handle);

  if (validSize > 0) {
    // signal advance to return pointer to first record
    CUpti_Activity *activity = NULL;
    bool status = false;
    size_t processed = 0;
    do {
      status = cupti_buffer_cursor_advance(buffer, validSize, &activity);
      if (status) {
        cupti_activity_process(activity);
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
 CUcontext context,
 const CUpti_ActivityKind activity_kinds[],
 bool enable
)
{
  PRINT("enter cupti_set_monitoring\n");
  int failed = 0;
  int succeeded = 0;

  cupti_activity_enable_t action =
    (enable ? 
     CUPTI_FN_NAME(cuptiActivityEnableContext): 
     CUPTI_FN_NAME(cuptiActivityDisableContext));

  int i = 0;
  for (;;) {
    CUpti_ActivityKind activity_kind = activity_kinds[i++];
    if (activity_kind == CUPTI_ACTIVITY_KIND_INVALID) break;
    bool succ = action(context, activity_kind) == CUPTI_SUCCESS;
    if (succ) {
      if (enable) {
        PRINT("activity %d enable succeeded\n", activity_kind);
      } else {
        PRINT("activity %d disable succeeded\n", activity_kind);
      }
      succeeded++;
    } else {
      if (enable) {
        PRINT("activity %d enable failed\n", activity_kind);
      } else {
        PRINT("activity %d disable failed\n", activity_kind);
      }
      failed++;
    }
  }
  if (succeeded > 0) {
    if (failed == 0) return cupti_set_all;
    else return cupti_set_some;
  }
  PRINT("leave cupti_set_monitoring\n");
  return cupti_set_none;
}


//-------------------------------------------------------------
// tracing control 
//-------------------------------------------------------------

void 
cupti_trace_init
(
)
{
  cupti_activity_enabled.buffer_request = cupti_buffer_alloc;
  cupti_activity_enabled.buffer_complete = cupti_buffer_completion_callback;
}


void 
cupti_trace_start
(
)
{
  HPCRUN_CUPTI_CALL(cuptiActivityRegisterCallbacks,
		    (cupti_activity_enabled.buffer_request, 
		     cupti_activity_enabled.buffer_complete));
}


void 
cupti_trace_finalize
(
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
)
{
  cupti_load_callback = cupti_load_callback_cuda;
  cupti_unload_callback = cupti_unload_callback_cuda;
  cupti_correlation_callback = cupti_correlation_callback_cuda;
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
  PRINT("enter cupti_correlation_enable\n");
  cupti_correlation_enabled = true;

  // For unknown reasons, external correlation ids do not return using 
  // cuptiActivityEnableContext
  HPCRUN_CUPTI_CALL(cuptiActivityEnable, 
		    (CUPTI_ACTIVITY_KIND_EXTERNAL_CORRELATION));

  PRINT("exit cupti_correlation_enable\n");
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
  PRINT("enter cupti_pc_sampling_enable\n");
  cupti_pc_sampling_enabled = true;
  CUpti_ActivityPCSamplingConfig config;
  config.samplingPeriod = 0;
  config.samplingPeriod2 = frequency;
  config.size = sizeof(config);

  HPCRUN_CUPTI_CALL(cuptiActivityConfigurePCSampling, (context, &config));

  HPCRUN_CUPTI_CALL(cuptiActivityEnableContext, 
		    (context, CUPTI_ACTIVITY_KIND_PC_SAMPLING));

  PRINT("exit cupti_pc_sampling_enable\n");
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


//-------------------------------------------------------------
// General functions
//-------------------------------------------------------------

static void
cupti_unknown_process
(
 CUpti_Activity *activity
)    
{
  PRINT("Unknown activity kind %d\n", activity->kind);
}


static void
__attribute__ ((unused))
cupti_device_process
(
 CUpti_ActivityDevice2 *device
)
{
  PRINT("Device id %d\n", device->id);
}


static void
cupti_sample_process
(
 void *record
)
{
#if CUPTI_API_VERSION >= 10
  CUpti_ActivityPCSampling3 *sample = (CUpti_ActivityPCSampling3 *)record;
#else
  CUpti_ActivityPCSampling2 *sample = (CUpti_ActivityPCSampling2 *)record;
#endif
  PRINT("source %u, functionId %u, pc 0x%x, corr %u, "
   "samples %u, latencySamples %u, stallReason %u\n",
   sample->sourceLocatorId,
   sample->functionId,
   sample->pcOffset,
   sample->correlationId,
   sample->samples,
   sample->latencySamples,
   sample->stallReason);

  cupti_correlation_id_map_entry_t *cupti_entry = 
    cupti_correlation_id_map_lookup(sample->correlationId);
  if (cupti_entry != NULL) {
    uint64_t external_id = 
      cupti_correlation_id_map_entry_external_id_get(cupti_entry);
    cupti_function_id_map_entry_t *entry = 
      cupti_function_id_map_lookup(sample->functionId);
    if (entry != NULL) {
      uint32_t function_index = 
        cupti_function_id_map_entry_function_index_get(entry);
      uint32_t cubin_id = cupti_function_id_map_entry_cubin_id_get(entry);
      ip_normalized_t ip = 
        cubin_id_transform(cubin_id, function_index, sample->pcOffset);
      cct_addr_t frm = { .ip_norm = ip };
      cupti_host_op_map_entry_t *host_op_entry = 
        cupti_host_op_map_lookup(external_id);
      if (host_op_entry != NULL) {
        PRINT("external_id %d\n", external_id);
        if (!cupti_host_op_map_samples_increase(external_id, sample->samples)) {
          cupti_correlation_id_map_delete(sample->correlationId);
        }
        cct_node_t *host_op_node = 
          cupti_host_op_map_entry_host_op_node_get(host_op_entry);
        cct_node_t *cct_child = NULL;
        if ((cct_child = hpcrun_cct_insert_addr(host_op_node, &frm)) != NULL) {
          PRINT("frm %d\n", ip);
          cupti_record_t *record = 
            cupti_host_op_map_entry_record_get(host_op_entry);
          cupti_cupti_activity_apply((CUpti_Activity *)sample, cct_child, 
            record);
        }
      } else {
        PRINT("host_map_entry %d not found\n", external_id);
      }
    }
  }
}


static void
__attribute__ ((unused))
cupti_source_locator_process
(
 CUpti_ActivitySourceLocator *asl
)
{
  PRINT("Source Locator Id %d, File %s Line %d\n", 
    asl->id, asl->fileName, 
    asl->lineNumber);
}


static void
cupti_function_process
(
 CUpti_ActivityFunction *af
)
{
  PRINT("Function Id %u, ctx %u, moduleId %u, functionIndex %u, name %s\n",
   af->id,
   af->contextId,
   af->moduleId,
   af->functionIndex,
   af->name);
  cupti_function_id_map_insert(af->id, af->functionIndex, af->moduleId);
}


static void
cupti_sampling_record_info_process
(
 CUpti_ActivityPCSamplingRecordInfo *sri
)
{
  PRINT("corr %u, totalSamples %llu, droppedSamples %llu\n",
   sri->correlationId,
   (unsigned long long)sri->totalSamples,
   (unsigned long long)sri->droppedSamples);
  cupti_correlation_id_map_entry_t *cupti_entry = 
    cupti_correlation_id_map_lookup(sri->correlationId);
  if (cupti_entry != NULL) {
    uint64_t external_id = 
      cupti_correlation_id_map_entry_external_id_get(cupti_entry);
    cupti_host_op_map_entry_t *host_op_entry = 
      cupti_host_op_map_lookup(external_id);
    if (host_op_entry != NULL) {
      cupti_record_t *record = 
        cupti_host_op_map_entry_record_get(host_op_entry);
      cct_node_t *host_op_node = 
        cupti_host_op_map_entry_host_op_node_get(host_op_entry);
      cupti_cupti_activity_apply((CUpti_Activity *)sri, host_op_node, record);
    }
    // sample record info is the last record for a given correlation id
    if (!cupti_host_op_map_total_samples_update
      (external_id, sri->totalSamples - sri->droppedSamples)) {
      cupti_correlation_id_map_delete(sri->correlationId);
    }
  }
  hpcrun_stats_acc_samples_add(sri->totalSamples);
  hpcrun_stats_acc_samples_dropped_add(sri->droppedSamples);
}


static void
cupti_correlation_process
(
 CUpti_ActivityExternalCorrelation *ec
)
{
  uint32_t correlation_id = ec->correlationId;
  uint64_t external_id = ec->externalId;
  if (cupti_correlation_id_map_lookup(correlation_id) == NULL) {
    cupti_correlation_id_map_insert(correlation_id, external_id);
  } else {
    PRINT("External CorrelationId Replace %lu)\n", external_id);
    cupti_correlation_id_map_external_id_replace(correlation_id, external_id);
  }
  PRINT("External CorrelationId %lu\n", external_id);
  PRINT("CorrelationId %lu\n", correlation_id);
}


static void
cupti_memcpy_process
(
 CUpti_ActivityMemcpy *activity
)
{
  cupti_correlation_id_map_entry_t *cupti_entry = 
    cupti_correlation_id_map_lookup(activity->correlationId);
  if (cupti_entry != NULL) {
    uint64_t external_id = 
      cupti_correlation_id_map_entry_external_id_get(cupti_entry);
    cupti_host_op_map_entry_t *host_op_entry = 
      cupti_host_op_map_lookup(external_id);
    if (host_op_entry != NULL) {
      cupti_record_t *record = 
        cupti_host_op_map_entry_record_get(host_op_entry);
      cct_node_t *host_op_node = 
        cupti_host_op_map_entry_host_op_node_get(host_op_entry);
      cupti_cupti_activity_apply((CUpti_Activity *)activity, 
        host_op_node, record);
      //FIXME(keren): In OpenMP, an external_id may maps to multiple cct_nodes
      //cupti_host_op_map_delete(external_id);
    }
    cupti_correlation_id_map_delete(activity->correlationId);
  } else {
    PRINT("Memcpy copy CorrelationId %u cannot be found\n", 
      activity->correlationId);
  }
  PRINT("Memcpy copy CorrelationId %u\n", activity->correlationId);
  PRINT("Memcpy copy kind %u\n", activity->copyKind);
  PRINT("Memcpy copy bytes %u\n", activity->bytes);
}


static void
cupti_memcpy2_process
(
 CUpti_ActivityMemcpy2 *activity
)
{
  cupti_correlation_id_map_entry_t *cupti_entry = 
    cupti_correlation_id_map_lookup(activity->correlationId);
  if (cupti_entry != NULL) {
    uint64_t external_id = 
      cupti_correlation_id_map_entry_external_id_get(cupti_entry);
    cupti_host_op_map_entry_t *host_op_entry = 
      cupti_host_op_map_lookup(external_id);
    if (host_op_entry != NULL) {
      cct_node_t *host_op_node = 
        cupti_host_op_map_entry_host_op_node_get(host_op_entry);
      cupti_record_t *record = 
        cupti_host_op_map_entry_record_get(host_op_entry);
      cupti_cupti_activity_apply((CUpti_Activity *)activity, 
        host_op_node, record);
      //FIXME(keren): In OpenMP, an external_id may maps to multiple cct_nodes
      //cupti_host_op_map_delete(external_id);
    }
    cupti_correlation_id_map_delete(activity->correlationId);
  }
  PRINT("Memcpy2 copy CorrelationId %u\n", activity->correlationId);
  PRINT("Memcpy2 copy kind %u\n", activity->copyKind);
  PRINT("Memcpy2 copy bytes %u\n", activity->bytes);
}


static void
cupti_memset_process
(
 CUpti_ActivityMemset *activity
)
{
  cupti_correlation_id_map_entry_t *cupti_entry = cupti_correlation_id_map_lookup(activity->correlationId);
  if (cupti_entry != NULL) {
    uint64_t external_id = cupti_correlation_id_map_entry_external_id_get(cupti_entry);
    cupti_host_op_map_entry_t *host_op_entry = cupti_host_op_map_lookup(external_id);
    if (host_op_entry != NULL) {
      cct_node_t *host_op_node = cupti_host_op_map_entry_host_op_node_get(host_op_entry);
      cupti_record_t *record = cupti_host_op_map_entry_record_get(host_op_entry);
      cupti_cupti_activity_apply((CUpti_Activity *)activity, host_op_node, record);
      //FIXME(keren): In OpenMP, an external_id may maps to multiple cct_nodes
      //cupti_host_op_map_delete(external_id);
    }
    cupti_correlation_id_map_delete(activity->correlationId);
  }
  PRINT("Memset CorrelationId %u\n", activity->correlationId);
  PRINT("Memset kind %u\n", activity->memoryKind);
  PRINT("Memset bytes %u\n", activity->bytes);
}



static void
cupti_memctr_process
(
 CUpti_ActivityUnifiedMemoryCounter *activity
)
{
  // FIXME(keren): no correlationId field
  //cupti_correlation_id_map_entry_t *cupti_entry = cupti_correlation_id_map_lookup(activity->correlationId);
  //if (cupti_entry != NULL) {
  //  uint64_t external_id = cupti_correlation_id_map_entry_external_id_get(cupti_entry);
  //  cct_node_t *node = hpcrun_op_id_map_lookup(external_id);
  //  cupti_attribute_activity(activity, node);
  //}
  //PRINT("Unified memory copy\n");
}


static void
cupti_activityAPI_process
(
 CUpti_ActivityAPI *activity
)
{
  switch (activity->kind) {
    case CUPTI_ACTIVITY_KIND_DRIVER:
    {
      break;
    }
    case CUPTI_ACTIVITY_KIND_RUNTIME:
    {
      break;
    }
    default:
      break;
  }
}


static void
cupti_kernel_process
(
 CUpti_ActivityKernel4 *activity
)
{
  cupti_correlation_id_map_entry_t *cupti_entry = 
    cupti_correlation_id_map_lookup(activity->correlationId);
  if (cupti_entry != NULL) {
    if (cuda_device_map_lookup(activity->deviceId) == NULL) {
      cuda_device_map_insert(activity->deviceId);
    }
    cupti_correlation_id_map_kernel_update(activity->correlationId,
      activity->deviceId, activity->start, activity->end);
    uint64_t external_id = 
      cupti_correlation_id_map_entry_external_id_get(cupti_entry);
    cupti_host_op_map_entry_t *host_op_entry = 
      cupti_host_op_map_lookup(external_id);
    if (host_op_entry != NULL) {
      cct_node_t *host_op_node = 
        cupti_host_op_map_entry_host_op_node_get(host_op_entry);
      cupti_record_t *record = 
        cupti_host_op_map_entry_record_get(host_op_entry);
      cupti_cupti_activity_apply((CUpti_Activity *)activity, 
        host_op_node, record);
      // do not delete it because it shares external_id with activity samples
    }
  }
  PRINT("Kernel execution deviceId %u\n", activity->deviceId);
  PRINT("Kernel execution CorrelationId %u\n", activity->correlationId);
}


static void
cupti_synchronization_process
(
 CUpti_ActivitySynchronization *activity
)
{
  cupti_correlation_id_map_entry_t *cupti_entry = 
    cupti_correlation_id_map_lookup(activity->correlationId);
  if (cupti_entry != NULL) {
    uint64_t external_id = 
      cupti_correlation_id_map_entry_external_id_get(cupti_entry);
    cupti_host_op_map_entry_t *host_op_entry = 
      cupti_host_op_map_lookup(external_id);
    if (host_op_entry != NULL) {
      cct_node_t *host_op_node = 
        cupti_host_op_map_entry_host_op_node_get(host_op_entry);
      cupti_record_t *record = 
        cupti_host_op_map_entry_record_get(host_op_entry);
      cupti_cupti_activity_apply((CUpti_Activity *)activity, 
        host_op_node, record);
      //FIXME(keren): In OpenMP, an external_id may maps to multiple cct_nodes
      //cupti_host_op_map_delete(external_id);
    }
    cupti_correlation_id_map_delete(activity->correlationId);
  }
  PRINT("Synchronization CorrelationId %u\n", activity->correlationId);
}


static void
cupti_memory_process
(
 CUpti_ActivityMemory *activity
)
{
  PRINT("Memory process not implemented\n");
}


static void
cupti_instruction_process
(
 CUpti_Activity *activity
)
{
  uint32_t correlation_id = 0;
  uint32_t function_id = 0;
  uint32_t pc_offset = 0;
  if (activity->kind == CUPTI_ACTIVITY_KIND_GLOBAL_ACCESS) {
    CUpti_ActivityGlobalAccess3 *global_access = 
      (CUpti_ActivityGlobalAccess3 *)activity;
    correlation_id = global_access->correlationId;
    function_id = global_access->functionId;
    pc_offset = global_access->pcOffset;
  } else if (activity->kind == CUPTI_ACTIVITY_KIND_SHARED_ACCESS) {
    CUpti_ActivitySharedAccess *shared_access = 
      (CUpti_ActivitySharedAccess *)activity;
    correlation_id = shared_access->correlationId;
    function_id = shared_access->functionId;
    pc_offset = shared_access->pcOffset;
  } else if (activity->kind == CUPTI_ACTIVITY_KIND_BRANCH) {
    CUpti_ActivityBranch2 *branch = (CUpti_ActivityBranch2 *)activity;
    correlation_id = branch->correlationId;
    function_id = branch->functionId;
    pc_offset = branch->pcOffset;
    PRINT("Branch instruction\n");
  }
  cupti_correlation_id_map_entry_t *cupti_entry = 
    cupti_correlation_id_map_lookup(correlation_id);
  if (cupti_entry != NULL) {
    uint64_t external_id = 
      cupti_correlation_id_map_entry_external_id_get(cupti_entry);
    PRINT("external_id %d\n", external_id);
    cupti_function_id_map_entry_t *entry = 
      cupti_function_id_map_lookup(function_id);
    if (entry != NULL) {
      uint32_t function_index = 
        cupti_function_id_map_entry_function_index_get(entry);
      uint32_t cubin_id = cupti_function_id_map_entry_cubin_id_get(entry);
      ip_normalized_t ip = 
        cubin_id_transform(cubin_id, function_index, pc_offset);
      cct_addr_t frm = { .ip_norm = ip };
      cupti_host_op_map_entry_t *host_op_entry = 
        cupti_host_op_map_lookup(external_id);
      if (host_op_entry != NULL) {
        cct_node_t *host_op_node = 
          cupti_host_op_map_entry_host_op_node_get(host_op_entry);
        cct_node_t *cct_child = NULL;
        if ((cct_child = hpcrun_cct_insert_addr(host_op_node, &frm)) != NULL) {
          cupti_record_t *record = 
            cupti_host_op_map_entry_record_get(host_op_entry);
          cupti_cupti_activity_apply(activity, cct_child, record);
        }
      }
    }
  }
  PRINT("Instruction CorrelationId %u\n", correlation_id);
}


//******************************************************************************
// activity processing
//******************************************************************************

void
cupti_activity_process
(
 CUpti_Activity *activity
)
{
  switch (activity->kind) {

  case CUPTI_ACTIVITY_KIND_SOURCE_LOCATOR:
    break;

  case CUPTI_ACTIVITY_KIND_PC_SAMPLING:
    cupti_sample_process(activity);
    break;

  case CUPTI_ACTIVITY_KIND_PC_SAMPLING_RECORD_INFO:
    cupti_sampling_record_info_process(
      (CUpti_ActivityPCSamplingRecordInfo *) activity);
    break;

  case CUPTI_ACTIVITY_KIND_FUNCTION:
    cupti_function_process((CUpti_ActivityFunction *) activity);
    break;

  case CUPTI_ACTIVITY_KIND_EXTERNAL_CORRELATION: 
    cupti_correlation_process((CUpti_ActivityExternalCorrelation *) activity);
    break;

  case CUPTI_ACTIVITY_KIND_MEMCPY: 
    cupti_memcpy_process((CUpti_ActivityMemcpy *) activity);
    break;

  case CUPTI_ACTIVITY_KIND_MEMCPY2: 
    cupti_memcpy2_process((CUpti_ActivityMemcpy2 *) activity);
    break;

  case CUPTI_ACTIVITY_KIND_MEMSET:
    cupti_memset_process((CUpti_ActivityMemset *) activity);

  case CUPTI_ACTIVITY_KIND_UNIFIED_MEMORY_COUNTER:
    cupti_memctr_process((CUpti_ActivityUnifiedMemoryCounter *) activity);
    break;

  case CUPTI_ACTIVITY_KIND_DRIVER:
  case CUPTI_ACTIVITY_KIND_RUNTIME:
    cupti_activityAPI_process((CUpti_ActivityAPI *) activity);
    break;

  case CUPTI_ACTIVITY_KIND_KERNEL:
    cupti_kernel_process((CUpti_ActivityKernel4 *) activity);
    break;

  case CUPTI_ACTIVITY_KIND_SYNCHRONIZATION:
    cupti_synchronization_process((CUpti_ActivitySynchronization *) activity);
    break;

  case CUPTI_ACTIVITY_KIND_MEMORY:
    cupti_memory_process((CUpti_ActivityMemory *) activity);
    break;

  case CUPTI_ACTIVITY_KIND_SHARED_ACCESS:
  case CUPTI_ACTIVITY_KIND_GLOBAL_ACCESS:
  case CUPTI_ACTIVITY_KIND_BRANCH:
    cupti_instruction_process(activity);
    break;

  default:
    cupti_unknown_process(activity);
    break;
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
  HPCRUN_CUPTI_CALL(cuptiActivityFlushAll, (CUPTI_ACTIVITY_FLAG_FLUSH_FORCED));
}


void
cupti_device_flush(void *args)
{
  if (cupti_stop_flag) {
    cupti_stop_flag_unset();
    cupti_activity_flush();
    // TODO(keren): replace cupti with sth. called device queue
    cupti_worker_activity_apply(cupti_activity_handle);
  }
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
cupti_device_shutdown(void *args)
{
  cupti_activity_flush();
  cupti_worker_activity_apply(cupti_activity_handle);
  cupti_callbacks_unsubscribe();
}


//******************************************************************************
// stack functions
//******************************************************************************

void
cupti_notification_handle(cupti_node_t *node)
{
  if (node->type == CUPTI_ENTRY_TYPE_NOTIFICATION) {
    cupti_entry_notification_t *notification_entry = 
      (cupti_entry_notification_t *)node->entry;
    PRINT("Insert external id %d\n", notification_entry->host_op_id);
    cupti_host_op_map_insert
      (notification_entry->host_op_id, notification_entry->cct_node, 
       notification_entry->record);
  }
}


void
cupti_activity_handle(cupti_node_t *node)
{
  if (node->type == CUPTI_ENTRY_TYPE_ACTIVITY) {
    cupti_entry_activity_t *activity_entry = 
      (cupti_entry_activity_t *)node->entry;
    cupti_activity_attribute(&(activity_entry->activity), 
      activity_entry->cct_node);
  }
}
