// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//***************************************************************************
//
// File:
//   gtpin-hpcrun-api.h
//
// Purpose:
//   define structure used to import hpctoolkit functions into hpctoolkit's
//   gtpin instrumentation library
//
//***************************************************************************

#ifndef gtpin_hpcrun_api_h
#define gtpin_hpcrun_api_h


//*****************************************************************************
// local include files
//*****************************************************************************

#include "../../../activity/gpu-op-placeholders.h"

#include "../../common/gpu-instrumentation.h"
#include "../../../activity/gpu-op-placeholders.h"
#include "../../../../cct/cct.h"

#include "../../../../safe-sampling.h"
#include "../../../activity/gpu-activity-channel.h"
#include "../../common/gpu-binary.h"
#include "../../../gpu-monitoring-thread-api.h"
#include "../../../operation/gpu-operation-multiplexer.h"
#include "../../../../utilities/hpcrun-nanotime.h"
#include "../../../gpu-metrics.h"

#ifndef __cplusplus
#include <stdatomic.h>
#else
#include <atomic>
#endif


//*****************************************************************************
// local include files
//*****************************************************************************

typedef struct gtpin_hpcrun_api_t {
  int (*safe_enter)(void);
  void (*safe_exit)(void);
  void (*real_exit)(int code);

  gpu_binary_kind_t (*binary_kind)(const char *mem_ptr, size_t mem_size);
  void (*gpu_binary_path_generate)(const char *, char *, char *);
  bool (*gpu_binary_store)(const char *, const void *, size_t);
  uint32_t (*gpu_binary_loadmap_insert)(const char *, bool);

  block_metrics_t (*fetch_block_metrics)(cct_node_t *);
  void (*attribute_instruction_metrics)(cct_node_t *, instruction_metrics_t);

  void (*get_cct_node_id)(cct_node_t *, uint16_t *, uintptr_t *);

  void (*cstack_ptr_set)(s_element_ptr_t *, s_element_t *);
  gpu_activity_channel_t *(*gpu_activity_channel_get_local)(void);

  void (*gpu_operation_multiplexer_push)
  (gpu_activity_channel_t *,
#ifdef __cplusplus
  std::
#endif
  atomic_int *, gpu_activity_t *);

  void (*hpcrun_thread_init_mem_pool_once)
  (int, cct_ctxt_t *, hpcrun_trace_type_t, bool);

  cct_node_t *(*hpcrun_cct_insert_ip_norm)(cct_node_t *, ip_normalized_t, bool);
  cct_node_t* (*hpcrun_cct_parent)(cct_node_t* node);
  void (*hpcrun_cct_walk_node_1st)(cct_node_t *, cct_op_t, cct_op_arg_t);

  uint64_t (*hpcrun_nanotime)(void);

  cct_node_t *(*gpu_op_ccts_get)(gpu_op_ccts_t *, gpu_placeholder_type_t);

  int (*crypto_compute_hash_string)(const void *, size_t, char *, unsigned int);

} gtpin_hpcrun_api_t;

#endif
