// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef level0_handle_map
#define level0_handle_map

//*****************************************************************************
// system includes
//*****************************************************************************

#include <stdint.h>

//*****************************************************************************
// local includes
//*****************************************************************************

#include "level0-data-node.h"

//******************************************************************************
// type declarations
//******************************************************************************

typedef struct level0_handle_map_entry_t level0_handle_map_entry_t;

//*****************************************************************************
// interface operations
//*****************************************************************************

level0_handle_map_entry_t *
level0_handle_map_lookup
(
 level0_handle_map_entry_t** map_root_ptr,
 uint64_t key
);

void
level0_handle_map_insert
(
 level0_handle_map_entry_t** map_root_ptr,
 level0_handle_map_entry_t* new_entry
);

void
level0_handle_map_delete
(
 level0_handle_map_entry_t** map_root_ptr,
 level0_handle_map_entry_t** free_list_ptr,
 uint64_t key
);

level0_handle_map_entry_t *
level0_handle_map_entry_new
(
 level0_handle_map_entry_t** free_list_ptr,
 uint64_t key,
 level0_data_node_t* data
);

level0_data_node_t**
level0_handle_map_entry_data_get
(
 level0_handle_map_entry_t *entry
);

#endif
