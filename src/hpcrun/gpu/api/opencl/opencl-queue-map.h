// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef opencl_queue_map_h
#define opencl_queue_map_h


//*****************************************************************************
// system includes
//*****************************************************************************

#include <stdint.h>



//*****************************************************************************
// type definitions
//*****************************************************************************

typedef struct opencl_queue_map_entry_t opencl_queue_map_entry_t;

//*****************************************************************************
// interface operations
//*****************************************************************************

opencl_queue_map_entry_t *
opencl_cl_queue_map_lookup
(
 uint64_t
);


uint32_t
opencl_cl_queue_map_update
(
 uint64_t,
 uint32_t
);


void
opencl_cl_queue_map_delete
(
 uint64_t
);


uint32_t
opencl_cl_queue_map_entry_queue_id_get
(
 opencl_queue_map_entry_t *entry
);


uint32_t
opencl_cl_queue_map_entry_context_id_get
(
 opencl_queue_map_entry_t *
);


#endif
