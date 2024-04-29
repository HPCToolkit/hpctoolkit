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
// Copyright ((c)) 2002-2024, Rice University
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

#ifndef gpu_context_id_map_h
#define gpu_context_id_map_h



//******************************************************************************
// system includes
//******************************************************************************

#include <stdint.h>



//******************************************************************************
// local includes
//******************************************************************************

#include "gpu-stream-id-map.h"



//******************************************************************************
// type declarations
//******************************************************************************

typedef struct gpu_context_id_map_entry_t gpu_context_id_map_entry_t;



//******************************************************************************
// interface declarations
//******************************************************************************

gpu_context_id_map_entry_t *
gpu_context_id_map_lookup
(
 uint32_t context_id
);


gpu_context_id_map_entry_t *
gpu_context_id_map_entry_new
(
 uint32_t context_id
);


_Bool
gpu_context_id_map_insert_entry
(
 gpu_context_id_map_entry_t *entry
);


void
gpu_context_id_map_context_delete
(
 uint32_t context_id
);


gpu_stream_id_map_entry_t **
gpu_context_id_map_entry_get_streams
(
  gpu_context_id_map_entry_t *entry
);


void
gpu_context_id_map_set_cpu_gpu_timestamp
(
  uint64_t t1,
  uint64_t t2
);


void
gpu_context_id_map_adjust_times
(
 gpu_context_id_map_entry_t *entry,
 gpu_trace_item_t *ti
);


#endif
