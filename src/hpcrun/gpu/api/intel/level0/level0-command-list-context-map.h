// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef level0_commandlist_context_map_h
#define level0_commandlist_context_map_h

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

ze_context_handle_t
level0_commandlist_context_map_lookup
(
 ze_command_list_handle_t command_list_handle
);

void
level0_commandlist_context_map_insert
(
 ze_command_list_handle_t command_list_handle,
 ze_context_handle_t hContext
);

void
level0_commandlist_context_map_delete
(
 ze_command_list_handle_t command_list_handle
);
#endif
