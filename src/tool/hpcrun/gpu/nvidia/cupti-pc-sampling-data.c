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

#define DEBUG 0

#include <stddef.h>
#include <pthread.h>

//******************************************************************************
// local includes
//******************************************************************************

#include "cupti-pc-sampling-data.h"

#include "../gpu-print.h"

#include <lib/prof-lean/stacks.h>

#include <hpcrun/memory/hpcrun-malloc.h>

#define stack_pop   \
  typed_stack_pop(cupti_pc_sampling_data_t, cstack)

#define stack_push   \
  typed_stack_push(cupti_pc_sampling_data_t, cstack)

#define stack_init  \
  typed_stack_elem_ptr_set(cupti_pc_sampling_data_t, cstack)

//******************************************************************************
// type declarations
//******************************************************************************

typedef struct cupti_pc_sampling_data_t {
  s_element_ptr_t next;

  CUpti_PCSamplingData *buffer_pc;
} typed_stack_elem(cupti_pc_sampling_data_t);

typed_stack_declare_type(cupti_pc_sampling_data_t);

typed_stack_impl(cupti_pc_sampling_data_t, cstack);

static typed_stack_elem_ptr(cupti_pc_sampling_data_t) pc_sampling_data_buffer;

//******************************************************************************
// interface operations 
//******************************************************************************


static CUpti_PCSamplingData *
cupti_pc_sampling_data_alloc
(
 size_t num_pcs,
 size_t num_stall_reasons
)
{
  CUpti_PCSamplingData *data = (CUpti_PCSamplingData *)hpcrun_malloc_safe(sizeof(CUpti_PCSamplingData));
  memset(data, 0, sizeof(CUpti_PCSamplingData));

  data->size = sizeof(CUpti_PCSamplingData);
  data->collectNumPcs = num_pcs;
  data->pPcData = (CUpti_PCSamplingPCData *)hpcrun_malloc_safe(num_pcs * sizeof(CUpti_PCSamplingPCData));
  memset(data->pPcData, 0, num_pcs * sizeof(CUpti_PCSamplingPCData));

  for (size_t i = 0; i < num_pcs; i++) {
    data->pPcData[i].stallReason = (CUpti_PCSamplingStallReason *)hpcrun_malloc_safe(
      num_stall_reasons * sizeof(CUpti_PCSamplingStallReason));
    memset(data->pPcData[i].stallReason, 0, num_stall_reasons * sizeof(CUpti_PCSamplingStallReason));
  }

  return data;
}


cupti_pc_sampling_data_t *
cupti_pc_sampling_data_produce
(
 size_t num_pcs,
 size_t num_stall_reasons
)
{
  cupti_pc_sampling_data_t *data = stack_pop(&pc_sampling_data_buffer);

  if (data == NULL) {
    data = (cupti_pc_sampling_data_t *)hpcrun_malloc_safe(sizeof(cupti_pc_sampling_data_t));
    stack_init(data, 0);
  }

  if (data->buffer_pc == NULL) {
    // Keep allocating, no limit
    data->buffer_pc = cupti_pc_sampling_data_alloc(num_pcs, num_stall_reasons);
  } else {
    PRINT("Reuse buffer_pc num_pcs %zu\n", num_pcs);
  }

  return data;
}


void
cupti_pc_sampling_data_free
(
 void *data
)
{
  stack_push(&pc_sampling_data_buffer, (cupti_pc_sampling_data_t *)data);
}


CUpti_PCSamplingData *
cupti_pc_sampling_buffer_pc_get
(
 cupti_pc_sampling_data_t *data
)
{
  return data->buffer_pc;
}
