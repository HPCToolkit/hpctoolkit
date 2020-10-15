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
// Copyright ((c)) 2002-2020, Rice University
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

//******************************************************************************
// system includes
//******************************************************************************

#include "amd-dbgapi.h"

#include <sys/types.h>
#include <unistd.h>

//******************************************************************************
// local includes
//******************************************************************************

#include "rocm-debug-api.h"

#include <hpcrun/safe-sampling.h>
#include <hpcrun/sample-sources/libdl.h>
#include <hpcrun/memory/hpcrun-malloc.h>

//******************************************************************************
// macros
//******************************************************************************

#define FORALL_ROCM_DEBUG_ROUTINES(macro)			\
  macro(amd_dbgapi_initialize)   \
  macro(amd_dbgapi_process_attach)   \
  macro(amd_dbgapi_process_detach) \
  macro(amd_dbgapi_code_object_list) \
  macro(amd_dbgapi_code_object_get_info)


#define ROCM_DEBUG_FN_NAME(f) DYN_FN_NAME(f)

#define ROCM_DEBUG_FN(fn, args) \
  static amd_dbgapi_status_t (*ROCM_DEBUG_FN_NAME(fn)) args

#define HPCRUN_ROCM_DEBUG_CALL(fn, args) \
{      \
  amd_dbgapi_status_t ret = ROCM_DEBUG_FN_NAME(fn) args;	\
  check_rocm_debug_status(ret, __LINE__); \
}

//******************************************************************************
// debug print
//******************************************************************************

#define DEBUG 0

#include "hpcrun/gpu/gpu-print.h"

//******************************************************************************
// local variables
//******************************************************************************

static amd_dbgapi_callbacks_t callbacks;
static amd_dbgapi_process_id_t self;
static amd_dbgapi_code_object_id_t *code_objects_id;

//----------------------------------------------------------
// rocm debug api function pointers for late binding
//----------------------------------------------------------

ROCM_DEBUG_FN
(
  amd_dbgapi_initialize,
  (
    amd_dbgapi_callbacks_t*
  )
);

ROCM_DEBUG_FN
(
  amd_dbgapi_process_attach,
  (
    amd_dbgapi_client_process_id_t,
    amd_dbgapi_process_id_t *
  )
);

ROCM_DEBUG_FN
(
  amd_dbgapi_process_detach,
  (
    amd_dbgapi_process_id_t
  )
);

ROCM_DEBUG_FN
(
  amd_dbgapi_code_object_list,
  (
    amd_dbgapi_process_id_t,
    size_t *,
    amd_dbgapi_code_object_id_t **,
    amd_dbgapi_changed_t *
  )
);

ROCM_DEBUG_FN
(
  amd_dbgapi_code_object_get_info,
  (
    amd_dbgapi_process_id_t,
    amd_dbgapi_code_object_id_t,
    amd_dbgapi_code_object_info_t,
    size_t,
    void*
  )
);

//******************************************************************************
// private operations
//******************************************************************************

static amd_dbgapi_status_t
hpcrun_self_process
(
  amd_dbgapi_client_process_id_t cp,
  amd_dbgapi_os_pid_t *os_pid
)
{
  *os_pid = getpid();
  return AMD_DBGAPI_STATUS_SUCCESS;
}

static amd_dbgapi_status_t
hpcrun_enable_notify_shared_library
(
  amd_dbgapi_client_process_id_t client_process_id,
  const char *shared_library_name,
  amd_dbgapi_shared_library_id_t shared_library_id,
  amd_dbgapi_shared_library_state_t *shared_library_state
)
{
  if (strcmp(shared_library_name, "libhsa-runtime64.so.1") == 0)
    *shared_library_state = AMD_DBGAPI_SHARED_LIBRARY_STATE_LOADED;
  else
    *shared_library_state = AMD_DBGAPI_SHARED_LIBRARY_STATE_UNLOADED;
  return AMD_DBGAPI_STATUS_SUCCESS;
}

static amd_dbgapi_status_t
hpcrun_disable_notify_shared_library
(
  amd_dbgapi_client_process_id_t client_process_id,
  amd_dbgapi_shared_library_id_t shared_library_id
)
{
  return AMD_DBGAPI_STATUS_SUCCESS;
}

static amd_dbgapi_status_t
hpcrun_get_symbol_address
(
  amd_dbgapi_client_process_id_t client_process_id,
  amd_dbgapi_shared_library_id_t shared_library_id,
  const char *symbol_name,
  amd_dbgapi_global_address_t *address
)
{
  // It is necessary to allow rocm debug library to call dlsym through this function.
  // We need to ensure that this code will not be called in a signal handler
  *address = (amd_dbgapi_global_address_t) dlsym(RTLD_DEFAULT, symbol_name);
  return AMD_DBGAPI_STATUS_SUCCESS;
}

static amd_dbgapi_status_t
hpcrun_insert_breakpoint
(
  amd_dbgapi_client_process_id_t client_process_id,
  amd_dbgapi_shared_library_id_t shared_library_id,
  amd_dbgapi_global_address_t address,
  amd_dbgapi_breakpoint_id_t breakpoint_id
)
{
  return AMD_DBGAPI_STATUS_SUCCESS;
}

static amd_dbgapi_status_t
hpcrun_remove_breakpoint
(
  amd_dbgapi_client_process_id_t client_process_id,
  amd_dbgapi_breakpoint_id_t breakpoint_id
)
{
  return AMD_DBGAPI_STATUS_SUCCESS;
}

static void
hpcrun_log_message
(
  amd_dbgapi_log_level_t level,
  const char *message
)
{
  PRINT("%s\n", message);
}

static void
check_rocm_debug_status
(
  amd_dbgapi_status_t ret,
  int lineNo
)
{
  if (ret == AMD_DBGAPI_STATUS_SUCCESS) {
    return;
  }

#define CHECK_RET(x) case x: { PRINT("%s", #x); break; }
  switch(ret) {
    CHECK_RET(AMD_DBGAPI_STATUS_FATAL)
    CHECK_RET(AMD_DBGAPI_STATUS_ERROR_NOT_INITIALIZED)
    CHECK_RET(AMD_DBGAPI_STATUS_ERROR_INVALID_PROCESS_ID)
    CHECK_RET(AMD_DBGAPI_STATUS_ERROR_INVALID_ARGUMENT)
    CHECK_RET(AMD_DBGAPI_STATUS_ERROR_CLIENT_CALLBACK)
    CHECK_RET(AMD_DBGAPI_STATUS_ERROR_INVALID_CODE_OBJECT_ID)
    CHECK_RET(AMD_DBGAPI_STATUS_ERROR_INVALID_ARGUMENT_SIZE)
    default:
      PRINT("unknown rocm debug return value");
      break;
  } 

#undef CHECK_RET

  PRINT(" at line %d\n", lineNo);
}

//******************************************************************************
// interface operations
//******************************************************************************

int
rocm_debug_api_bind
(
  void
)
{
  // This disable HIP's deferred code object loading.
  // We can remove this when we start to use HSA API tracing
  setenv("HIP_ENABLE_DEFERRED_LOADING", "0", 1);

#ifndef HPCRUN_STATIC_LINK
  // dynamic libraries only availabile in non-static case
  hpcrun_force_dlopen(true);
  CHK_DLOPEN(rocm_debug, "librocm-dbgapi.so", RTLD_NOW | RTLD_GLOBAL);
  hpcrun_force_dlopen(false);

#define ROCM_DEBUG_BIND(fn) \
  CHK_DLSYM(rocm_debug, fn);

  FORALL_ROCM_DEBUG_ROUTINES(ROCM_DEBUG_BIND)

#undef ROCM_DEBUG_BIND
  return 0;
#else
  return -1;
#endif // ! HPCRUN_STATIC_LINK
}

void
rocm_debug_api_init
(
  void
)
{
  // Fill in call back functions for rocm debug api
  callbacks.allocate_memory = malloc;
  callbacks.deallocate_memory = free;
  callbacks.get_os_pid = hpcrun_self_process;
  callbacks.enable_notify_shared_library = hpcrun_enable_notify_shared_library;
  callbacks.disable_notify_shared_library = hpcrun_disable_notify_shared_library;
  callbacks.get_symbol_address = hpcrun_get_symbol_address;
  callbacks.insert_breakpoint = hpcrun_insert_breakpoint;
  callbacks.remove_breakpoint = hpcrun_remove_breakpoint;
  callbacks.log_message = hpcrun_log_message;

  HPCRUN_ROCM_DEBUG_CALL(amd_dbgapi_initialize, (&callbacks));
  HPCRUN_ROCM_DEBUG_CALL(amd_dbgapi_process_attach,
    ((amd_dbgapi_client_process_id_t)(&self), &self));
}

void
rocm_debug_api_fini
(
  void
)
{
  HPCRUN_ROCM_DEBUG_CALL(amd_dbgapi_process_detach, (self));
}

void
rocm_debug_api_query_code_object
(
  size_t* code_object_count_ptr
)
{
  HPCRUN_ROCM_DEBUG_CALL(amd_dbgapi_code_object_list,
    (self, code_object_count_ptr, &code_objects_id, NULL));
  PRINT("code object count %u\n", *code_object_count_ptr);
}

char*
rocm_debug_api_query_uri
(
  size_t code_object_index
)
{
  char* uri;
  HPCRUN_ROCM_DEBUG_CALL(amd_dbgapi_code_object_get_info,
    (self, code_objects_id[code_object_index],
      AMD_DBGAPI_CODE_OBJECT_INFO_URI_NAME,
      sizeof(char*), (void*)(&uri)));
  return uri;
}
