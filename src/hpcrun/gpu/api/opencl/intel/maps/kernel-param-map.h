// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef kernel_param_map_h
#define kernel_param_map_h


//*****************************************************************************
// system includes
//*****************************************************************************

#include <stdint.h>



//*****************************************************************************
// local includes
//*****************************************************************************

#include <CL/cl.h>



//*****************************************************************************
// type definitions
//*****************************************************************************

typedef struct kernel_param_map_entry_t kernel_param_map_entry_t;

typedef struct kp_node_t {
  cl_mem mem;
  size_t size;
  struct kp_node_t *next;
} kp_node_t;



//*****************************************************************************
// interface operations
//*****************************************************************************

kernel_param_map_entry_t *
kernel_param_map_lookup
(
 uint64_t
);


kernel_param_map_entry_t*
kernel_param_map_insert
(
 uint64_t,
 const void *,
 size_t
);


void
kernel_param_map_delete
(
 uint64_t
);


uint64_t
kernel_param_map_entry_kernel_id_get
(
 kernel_param_map_entry_t *entry
);


kp_node_t*
kernel_param_map_entry_kp_list_get
(
 kernel_param_map_entry_t *entry
);

#endif  // kernel_param_map_h
