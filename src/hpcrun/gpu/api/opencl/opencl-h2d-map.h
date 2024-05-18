// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef opencl_h2d_map_h
#define opencl_h2d_map_h


//*****************************************************************************
// system includes
//*****************************************************************************

#include <stdint.h>



//*****************************************************************************
// local includes
//*****************************************************************************

#include "opencl-memory-manager.h"



//*****************************************************************************
// type definitions
//*****************************************************************************

typedef struct opencl_h2d_map_entry_t opencl_h2d_map_entry_t;


typedef void (*opencl_splay_fn_t)
(
        opencl_h2d_map_entry_t *,
        splay_visit_t,
        void *
);



//*****************************************************************************
// interface operations
//*****************************************************************************

opencl_h2d_map_entry_t *
opencl_h2d_map_lookup
(
 uint64_t
);


void
opencl_h2d_map_insert
(
 uint64_t,
 uint64_t,
 size_t,
 opencl_object_t *
);


void
opencl_h2d_map_delete
(
 uint64_t
);


uint64_t
opencl_h2d_map_entry_buffer_id_get
(
 opencl_h2d_map_entry_t *entry
);


uint64_t
opencl_h2d_map_entry_correlation_get
(
 opencl_h2d_map_entry_t *
);


size_t
opencl_h2d_map_entry_size_get
(
 opencl_h2d_map_entry_t *
);


opencl_object_t *
opencl_h2d_map_entry_callback_info_get
(
 opencl_h2d_map_entry_t *entry
);


void
opencl_update_ccts_for_h2d_nodes
(
 opencl_splay_fn_t fn
);


uint64_t
opencl_h2d_map_count
(
 void
);

#endif
