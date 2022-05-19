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

#ifndef cupti_context_map_h
#define cupti_context_map_h

//*****************************************************************************
// system includes
//*****************************************************************************

#include <stdint.h>

//*****************************************************************************
// local includes
//*****************************************************************************

#include <cuda.h>

#include "cupti-pc-sampling-data.h"

//*****************************************************************************
// type definitions 
//*****************************************************************************

typedef struct cupti_context_map_entry_s cupti_context_map_entry_t;

typedef void (*cupti_context_process_fn_t)
(
 CUcontext context,
 void *args
);

typedef struct cupti_context_process_args_t {
  cupti_context_process_fn_t fn;
  void *args;
} cupti_context_process_args_t;

//*****************************************************************************
// interface operations
//*****************************************************************************

cupti_context_map_entry_t *
cupti_context_map_lookup
(
 CUcontext context
); 


void
cupti_context_map_init
(
 CUcontext context
);


void
cupti_context_map_pc_sampling_insert
(
 CUcontext context,
 size_t num_stall_reasons,
 uint32_t *stall_reason_index,
 char **stall_reason_names,
 size_t num_buffer_pcs,
 cupti_pc_sampling_data_t *buffer_pc
);


void
cupti_context_map_delete
(
 CUcontext context
);


cupti_pc_sampling_data_t *
cupti_context_map_entry_pc_sampling_data_get
(
 cupti_context_map_entry_t *entry
);


void
cupti_context_map_process
(
 cupti_context_process_fn_t fn,
 void *args
);


size_t
cupti_context_map_entry_num_stall_reasons_get
(
 cupti_context_map_entry_t *entry
);


uint32_t *
cupti_context_map_entry_stall_reason_index_get
(
 cupti_context_map_entry_t *entry
);


char **
cupti_context_map_entry_stall_reason_names_get
(
 cupti_context_map_entry_t *entry
);

#endif
