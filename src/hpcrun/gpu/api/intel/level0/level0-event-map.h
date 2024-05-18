// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef level0_event_map_h
#define level0_event_map_h


//*****************************************************************************
// local includes
//*****************************************************************************

#include "level0-api.h"
#include "level0-data-node.h"


//*****************************************************************************
// interface operations
//*****************************************************************************

level0_data_node_t*
level0_event_map_lookup
(
 ze_event_handle_t event_handle
);

void
level0_event_map_insert
(
 ze_event_handle_t event_handle,
 level0_data_node_t* new_entry
);

void
level0_event_map_delete
(
 ze_event_handle_t event_handle
);

#endif
