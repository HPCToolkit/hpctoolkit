// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef gpu_op_placeholders_h
#define gpu_op_placeholders_h



//******************************************************************************
// system includes
//******************************************************************************

#include <stdint.h>



//******************************************************************************
// local includes
//******************************************************************************

#include "../../utilities/ip-normalized.h"
#include "../../cct/cct.h"



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
