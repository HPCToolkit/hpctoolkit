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

#ifndef gpu_op_placeholders_h
#define gpu_op_placeholders_h



//******************************************************************************
// system includes
//******************************************************************************

#include <stdint.h>



//******************************************************************************
// local includes
//******************************************************************************

#include <hpcrun/utilities/ip-normalized.h>
#include <hpcrun/hpcrun-placeholders.h>



//******************************************************************************
// type declarations
//******************************************************************************

typedef enum gpu_placeholder_type_t {
  gpu_placeholder_type_copy    = 0, // general copy, d2d d2a, or a2d
  gpu_placeholder_type_copyin  = 1,
  gpu_placeholder_type_copyout = 2,
  gpu_placeholder_type_alloc   = 3,
  gpu_placeholder_type_delete  = 4,
  gpu_placeholder_type_kernel  = 5,
  gpu_placeholder_type_memset  = 6,
  gpu_placeholder_type_sync    = 7,
  gpu_placeholder_type_trace   = 8,
  gpu_placeholder_type_count   = 9 
} gpu_placeholder_type_t;


typedef uint32_t gpu_op_placeholder_flags_t;


typedef struct gpu_op_ccts_t {
  cct_node_t *ccts[gpu_placeholder_type_count];
} gpu_op_ccts_t;



//******************************************************************************
// public data
//******************************************************************************

extern gpu_op_placeholder_flags_t gpu_op_placeholder_flags_all;
extern gpu_op_placeholder_flags_t gpu_op_placeholder_flags_none;



//******************************************************************************
// interface operations
//******************************************************************************

ip_normalized_t
gpu_op_placeholder_ip
(
 gpu_placeholder_type_t type
);


// this function implements bulk insertion. ccts for all placeholders
// with flags set will be inserted as children of api_node. ccts for
// all placeholders whose flags are 0 will be initialized to null.
void
gpu_op_ccts_insert
(
 cct_node_t *api_node,
 gpu_op_ccts_t *gpu_op_ccts,
 gpu_op_placeholder_flags_t flags
);


cct_node_t *
gpu_op_ccts_get
(
 gpu_op_ccts_t *gpu_op_ccts,
 gpu_placeholder_type_t type
);


void
gpu_op_placeholder_flags_set
(
 gpu_op_placeholder_flags_t *flags,
 gpu_placeholder_type_t type
);


bool
gpu_op_placeholder_flags_is_set
(
 gpu_op_placeholder_flags_t flags,
 gpu_placeholder_type_t type
);



#endif
