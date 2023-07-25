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
// Copyright ((c)) 2002-2023, Rice University
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
//   gtpin-shim.c
//
// Purpose:
//   load the HPCToolkit C++ helper library for interacting with GTPin
//
//***************************************************************************

//*****************************************************************************
// system include files
//*****************************************************************************

#include <pthread.h>



//*****************************************************************************
// local include files
//*****************************************************************************

#include "gtpin-hpcrun-api.h"

#include <hpcrun/cct/cct.h>
#include <hpcrun/gpu/gpu-binary.h>
#include <hpcrun/gpu/gpu-activity-channel.h>
#include <hpcrun/gpu/gpu-correlation.h>
#include <hpcrun/gpu/gpu-instrumentation.h>
#include <hpcrun/gpu/gpu-metrics.h>
#include <hpcrun/gpu/gpu-monitoring-thread-api.h>
#include <hpcrun/gpu/gpu-op-placeholders.h>
#include <hpcrun/gpu/gpu-operation-multiplexer.h>
#include <hpcrun/messages/messages.h>
#include <hpcrun/safe-sampling.h>
#include <hpcrun/utilities/hpcrun-nanotime.h>

#include <lib/prof-lean/crypto-hash.h>

#include <monitor.h>



//*****************************************************************************
// local data
//*****************************************************************************

static gtpin_hpcrun_api_t gtpin_hpcrun_api = {
  .safe_enter = hpcrun_safe_enter_noinline,
  .safe_exit = hpcrun_safe_exit_noinline,
  .real_exit = monitor_real_exit,
  .binary_kind = gpu_binary_kind,
  .gpu_binary_path_generate = gpu_binary_path_generate,
  .gpu_operation_multiplexer_push = gpu_operation_multiplexer_push,
  .fetch_block_metrics = fetch_block_metrics,
  .get_cct_node_id = get_cct_node_id,
  .cstack_ptr_set = cstack_ptr_set,
  .hpcrun_nanotime = hpcrun_nanotime,
  .hpcrun_thread_init_mem_pool_once = hpcrun_thread_init_mem_pool_once,
  .gpu_activity_channel_get = gpu_activity_channel_get,
  .attribute_instruction_metrics = attribute_instruction_metrics,
  .crypto_compute_hash_string = crypto_compute_hash_string,
  .hpcrun_cct_insert_instruction_child = hpcrun_cct_insert_instruction_child,
  .gpu_op_ccts_get = gpu_op_ccts_get,
  .gpu_binary_loadmap_insert = gpu_binary_loadmap_insert,
  .gpu_binary_store = gpu_binary_store,
  .hpcrun_cct_insert_ip_norm = hpcrun_cct_insert_ip_norm,
  .hpcrun_cct_walk_node_1st = hpcrun_cct_walk_node_1st
};

static pthread_once_t once_control = PTHREAD_ONCE_INIT;

static void (*gtpin_instrumentation_options_fn)(gpu_instrumentation_t *);
static void (*gtpin_produce_runtime_callstack_fn)(gpu_op_ccts_t *);
static void (*gtpin_process_block_instructions_fn)(cct_node_t *);
static uintptr_t (*gtpin_lookup_kernel_ip_fn)(const char *kernel_name);



//*****************************************************************************
// private operations
//*****************************************************************************

static void init()
{
  Lmid_t scope = getenv("HPCRUN_GTPIN_VISIBLE") ? LM_ID_BASE : LM_ID_NEWLM;
  void *hpcrun_gtpinlib = dlmopen(scope, "libhpcrun_gtpin_cxx.so",
				  RTLD_LOCAL | RTLD_LAZY);
  if (hpcrun_gtpinlib == NULL) {
    EEMSG("FATAL: hpcrun failure: unable to load HPCToolkit's gtpin support library: %s", dlerror());
    monitor_real_exit(-1);
  }

  void *malloc_lib = dlopen("libtbbmalloc.so.2", RTLD_LAZY | RTLD_GLOBAL);
  if (malloc_lib == NULL) {
    EEMSG("FATAL: hpcrun failure: unable to load HPCToolkit's gtpin support library: %s", dlerror());
    monitor_real_exit(-1);
  }

  void (*gtpin_hpcrun_api_set_fn)(gtpin_hpcrun_api_t *) = dlsym(hpcrun_gtpinlib, "gtpin_hpcrun_api_set");
  if (gtpin_hpcrun_api_set_fn == NULL) {
    EEMSG("FATAL: hpcrun failure: unable to connect to HPCToolkit's gtpin support library: %s", dlerror());
    monitor_real_exit(-1);
  }
  gtpin_hpcrun_api_set_fn(&gtpin_hpcrun_api);

  gtpin_instrumentation_options_fn = dlsym(hpcrun_gtpinlib, "gtpin_instrumentation_options");
  gtpin_produce_runtime_callstack_fn = dlsym(hpcrun_gtpinlib, "gtpin_produce_runtime_callstack");
  gtpin_process_block_instructions_fn = dlsym(hpcrun_gtpinlib, "gtpin_process_block_instructions");
  gtpin_lookup_kernel_ip_fn = dlsym(hpcrun_gtpinlib, "gtpin_lookup_kernel_ip");
}



//*****************************************************************************
// interface operations
//*****************************************************************************

void
gtpin_instrumentation_options
(
  gpu_instrumentation_t *instrumentation
)
{
  pthread_once(&once_control, init);
  gtpin_instrumentation_options_fn(instrumentation);
}


void
gtpin_produce_runtime_callstack
(
  gpu_op_ccts_t *op_ccts
)
{
  pthread_once(&once_control, init);
  gtpin_produce_runtime_callstack_fn(op_ccts);
}


void 
gtpin_process_block_instructions
(
  cct_node_t *node
)
{
  pthread_once(&once_control, init);
  gtpin_process_block_instructions_fn(node);
}


uintptr_t 
gtpin_lookup_kernel_ip
(
  const char *kernel_name
)
{
  pthread_once(&once_control, init);
  return gtpin_lookup_kernel_ip_fn(kernel_name);
}
