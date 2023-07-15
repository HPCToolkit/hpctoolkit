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
// vendor include file
//*****************************************************************************

#include <gtpin.h>



//*****************************************************************************
// local include files
//*****************************************************************************

#include <hpcrun/gpu/gpu-correlation.h>
#include <hpcrun/gpu/gpu-op-placeholders.h>

#include <hpcrun/gpu/gpu-instrumentation.h>
#include <hpcrun/gpu/gpu-op-placeholders.h>
#include <hpcrun/cct/cct.h>

#include <hpcrun/safe-sampling.h>
#include <hpcrun/gpu/gpu-activity-channel.h>
#include <hpcrun/gpu/gpu-binary.h>
#include <hpcrun/gpu/gpu-monitoring-thread-api.h>
#include <hpcrun/gpu/gpu-operation-multiplexer.h>
#include <hpcrun/utilities/hpcrun-nanotime.h>
#include <hpcrun/gpu/gpu-metrics.h>


//*****************************************************************************
// local include files
//*****************************************************************************

typedef struct gtpin_hpcrun_api_t {
  int (*safe_enter)(void);
  void (*safe_exit)(void);
  void (*real_exit)(int code);

  gpu_binary_kind_t (*binary_kind)(const char *mem_ptr, size_t mem_size);
  void (*gpu_binary_path_generate)(const char *, char *);
  bool (*gpu_binary_store)(const char *, const void *, size_t);
  uint32_t (*gpu_binary_loadmap_insert)(const char *, bool);

  block_metrics_t (*fetch_block_metrics)(cct_node_t *);
  void (*attribute_instruction_metrics)(cct_node_t *, instruction_metrics_t);

  void (*get_cct_node_id)(cct_node_t *, uint16_t *, uintptr_t *);

  void (*cstack_ptr_set)(s_element_ptr_t *, s_element_t *);
  gpu_activity_channel_t *(*gpu_activity_channel_get)(void);

  void (*gpu_operation_multiplexer_push)
  (gpu_activity_channel_t *, atomic_int *, gpu_activity_t *);

  void (*hpcrun_thread_init_mem_pool_once)
  (int, cct_ctxt_t *, hpcrun_trace_type_t, bool);

  cct_node_t *(*hpcrun_cct_insert_instruction_child)(cct_node_t *, int32_t);
  cct_node_t *(*hpcrun_cct_insert_ip_norm)(cct_node_t *, ip_normalized_t, bool);
  void (*hpcrun_cct_walk_node_1st)(cct_node_t *, cct_op_t, cct_op_arg_t);

  uint64_t (*hpcrun_nanotime)(void);

  cct_node_t *(*gpu_op_ccts_get)(gpu_op_ccts_t *, gpu_placeholder_type_t);

  int (*crypto_compute_hash_string)(const void *, size_t, char *, unsigned int);

} gtpin_hpcrun_api_t;

#endif
