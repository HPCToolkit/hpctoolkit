// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

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

#define _GNU_SOURCE

#include <pthread.h>



//*****************************************************************************
// local include files
//*****************************************************************************

#include "gtpin-hpcrun-api.h"

#include "../../../../audit/audit-api.h"
#include "../../../../cct/cct.h"
#include "../../common/gpu-binary.h"
#include "../../../activity/gpu-activity-channel.h"
#include "../../common/gpu-instrumentation.h"
#include "../../../gpu-metrics.h"
#include "../../../gpu-monitoring-thread-api.h"
#include "../../../activity/gpu-op-placeholders.h"
#include "../../../operation/gpu-operation-multiplexer.h"
#include "../../../../messages/messages.h"
#include "../../../../safe-sampling.h"
#include "../../../../utilities/hpcrun-nanotime.h"

#include "../../../../../common/lean/crypto-hash.h"

#include "../../../../libmonitor/monitor.h"



//*****************************************************************************
// local data
//*****************************************************************************

static void my_exit(int status) {
  auditor_exports->exit(status);
}

static gtpin_hpcrun_api_t gtpin_hpcrun_api = {
  .safe_enter = hpcrun_safe_enter_noinline,
  .safe_exit = hpcrun_safe_exit_noinline,
  .real_exit = my_exit,
  .binary_kind = gpu_binary_kind,
  .gpu_binary_path_generate = gpu_binary_path_generate,
  .gpu_operation_multiplexer_push = gpu_operation_multiplexer_push,
  .fetch_block_metrics = fetch_block_metrics,
  .get_cct_node_id = get_cct_node_id,
  .cstack_ptr_set = cstack_ptr_set,
  .hpcrun_nanotime = hpcrun_nanotime,
  .hpcrun_thread_init_mem_pool_once = hpcrun_thread_init_mem_pool_once,
  .gpu_activity_channel_get_local = gpu_activity_channel_get_local,
  .attribute_instruction_metrics = attribute_instruction_metrics,
  .crypto_compute_hash_string = crypto_compute_hash_string,
  .gpu_op_ccts_get = gpu_op_ccts_get,
  .gpu_binary_loadmap_insert = gpu_binary_loadmap_insert,
  .gpu_binary_store = gpu_binary_store,
  .hpcrun_cct_insert_ip_norm = hpcrun_cct_insert_ip_norm,
  .hpcrun_cct_parent = hpcrun_cct_parent,
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
    auditor_exports->exit(-1);
  }

  void *malloc_lib = dlopen("libtbbmalloc.so.2", RTLD_LAZY | RTLD_GLOBAL);
  if (malloc_lib == NULL) {
    EEMSG("FATAL: hpcrun failure: unable to load HPCToolkit's gtpin support library: %s", dlerror());
    auditor_exports->exit(-1);
  }

  void (*gtpin_hpcrun_api_set_fn)(gtpin_hpcrun_api_t *) = dlsym(hpcrun_gtpinlib, "gtpin_hpcrun_api_set");
  if (gtpin_hpcrun_api_set_fn == NULL) {
    EEMSG("FATAL: hpcrun failure: unable to connect to HPCToolkit's gtpin support library: %s", dlerror());
    auditor_exports->exit(-1);
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
