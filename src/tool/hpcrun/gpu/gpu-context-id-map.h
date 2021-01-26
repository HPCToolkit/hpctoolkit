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

#ifndef gpu_context_id_map_h
#define gpu_context_id_map_h



//******************************************************************************
// system includes
//******************************************************************************

#include <stdint.h>



//******************************************************************************
// local includes
//******************************************************************************

#include "gpu-trace.h"



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


void
gpu_context_id_map_context_delete
(
 uint32_t context_id
);


void
gpu_context_id_map_stream_delete
(
 uint32_t context_id,
 uint32_t stream_id
);


void
gpu_context_id_map_stream_process
(
 uint32_t context_id,
 uint32_t stream_id,
 gpu_trace_fn_t fn,
 gpu_trace_item_t *ti
);


void
gpu_context_id_map_context_process
(
 uint32_t context_id,
 gpu_trace_fn_t fn,
 gpu_trace_item_t *ti
);


void
gpu_context_id_map_device_process
(
 gpu_trace_fn_t fn,
 void *arg
);


void
gpu_context_stream_map_signal_all
(
 void
);



#endif

