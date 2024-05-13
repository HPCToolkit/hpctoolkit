// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef opencl_context_map_h
#define opencl_context_map_h


//*****************************************************************************
// system includes
//*****************************************************************************

#include <stdint.h>



//*****************************************************************************
// type definitions
//*****************************************************************************

typedef struct opencl_context_map_entry_t opencl_context_map_entry_t;



//*****************************************************************************
// interface operations
//*****************************************************************************

opencl_context_map_entry_t *
opencl_cl_context_map_lookup
(
 uint64_t
);


uint32_t
opencl_cl_context_map_update
(
 uint64_t
);


void
opencl_cl_context_map_delete
(
 uint64_t
);


uint32_t
opencl_cl_context_map_entry_context_id_get
(
 opencl_context_map_entry_t *entry
);


#endif
