// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef level0_commandlist_map_h
#define level0_commandlist_map_h

//*****************************************************************************
// system includes
//*****************************************************************************

#include <stdint.h>

//*****************************************************************************
// local includes
//*****************************************************************************

#include "level0-api.h"
#include "level0-handle-map.h"
#include "level0-data-node.h"

//*****************************************************************************
// interface operations
//*****************************************************************************

level0_data_node_t**
level0_commandlist_map_lookup
(
 ze_command_list_handle_t command_list_handle
);

void
level0_commandlist_map_insert
(
 ze_command_list_handle_t command_list_handle
);

void
level0_commandlist_map_delete
(
 ze_command_list_handle_t command_list_handle
);

level0_data_node_t*
level0_commandlist_alloc_kernel
(
 ze_kernel_handle_t kernel,
 ze_event_handle_t event,
 ze_event_pool_handle_t event_pool
);

level0_data_node_t*
level0_commandlist_alloc_memcpy
(
 ze_memory_type_t src_type,
 ze_memory_type_t dst_type,
 size_t copy_size,
 ze_event_handle_t event,
 ze_event_pool_handle_t event_pool
);

level0_data_node_t*
level0_commandlist_append_kernel
(
 level0_data_node_t** command_list,
 ze_kernel_handle_t kernel,
 ze_event_handle_t event,
 ze_event_pool_handle_t event_pool
);

level0_data_node_t*
level0_commandlist_append_memcpy
(
 level0_data_node_t** command_list,
 ze_memory_type_t src_type,
 ze_memory_type_t dst_type,
 size_t copy_size,
 ze_event_handle_t event,
 ze_event_pool_handle_t event_pool
);
#endif
