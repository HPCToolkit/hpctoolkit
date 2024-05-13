// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef opencl_kernel_loadmap_map_h
#define opencl_kernel_loadmap_map_h


//*****************************************************************************
// system includes
//*****************************************************************************

#include <stdint.h>



//*****************************************************************************
// local includes
//*****************************************************************************



//*****************************************************************************
// type definitions
//*****************************************************************************

typedef struct opencl_kernel_loadmap_map_entry_t opencl_kernel_loadmap_map_entry_t;



//*****************************************************************************
// interface operations
//*****************************************************************************

opencl_kernel_loadmap_map_entry_t *
opencl_kernel_loadmap_map_lookup
(
 uint64_t
);


void
opencl_kernel_loadmap_map_insert
(
 uint64_t,
 uint32_t
);


void
opencl_kernel_loadmap_map_delete
(
 uint64_t
);


uint64_t
opencl_kernel_loadmap_map_entry_kernel_name_id_get
(
 opencl_kernel_loadmap_map_entry_t *entry
);


uint32_t
opencl_kernel_loadmap_map_entry_module_id_get
(
 opencl_kernel_loadmap_map_entry_t *
);


uint64_t
opencl_kernel_loadmap_map_count
(
 void
);

#endif
