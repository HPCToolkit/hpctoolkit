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
// Copyright ((c)) 2002-2022, Rice University
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

#ifndef gpu_correlation_id_map_h
#define gpu_correlation_id_map_h



//*****************************************************************************
// system includes
//*****************************************************************************

#include <stdint.h>



//*****************************************************************************
// type definitions 
//*****************************************************************************

typedef struct gpu_correlation_id_map_entry_t gpu_correlation_id_map_entry_t;

typedef struct cct_node_t cct_node_t;



//*****************************************************************************
// interface operations
//*****************************************************************************

gpu_correlation_id_map_entry_t *
gpu_correlation_id_map_lookup
(
 uint64_t gpu_correlation_id
);


void
gpu_correlation_id_map_insert
(
 uint64_t gpu_correlation_id,
 uint64_t host_correlation_id
);


void
gpu_correlation_id_map_delete
(
 uint64_t gpu_correlation_id
);


void
gpu_correlation_id_map_external_id_replace
(
 uint64_t gpu_correlation_id,
 uint64_t host_correlation_id
);


void
gpu_correlation_id_map_kernel_update
(
 uint64_t correlation_id,
 uint32_t device_id,
 uint64_t start,
 uint64_t end
);


uint64_t
gpu_correlation_id_map_entry_external_id_get
(
 gpu_correlation_id_map_entry_t *entry
);


uint64_t
gpu_correlation_id_map_entry_start_get
(
 gpu_correlation_id_map_entry_t *entry
);


uint64_t
gpu_correlation_id_map_entry_end_get
(
 gpu_correlation_id_map_entry_t *entry
);


uint64_t
gpu_correlation_id_map_entry_device_id_get
(
 gpu_correlation_id_map_entry_t *entry
);



#endif
