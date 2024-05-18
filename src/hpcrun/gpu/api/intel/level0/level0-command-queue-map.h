// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef level0_command_queue_map_h
#define level0_command_queue_map_h

//*****************************************************************************
// local includes
//*****************************************************************************

#include "level0-api.h"

//******************************************************************************
// type declarations
//******************************************************************************

typedef struct {
  uint32_t num;
  ze_command_list_handle_t* list;
} level0_command_queue_data_t;

//******************************************************************************
// interface operations
//******************************************************************************

void
level0_command_queue_map_insert
(
  ze_command_queue_handle_t hcommand_queue,
  uint32_t numCommandLists,
  ze_command_list_handle_t* phCommandLists
);

level0_command_queue_data_t*
level0_command_queue_map_lookup
(
  ze_command_queue_handle_t hcommand_queue
);

void
level0_command_queue_map_delete
(
  ze_command_queue_handle_t hcommand_queue
);

#endif
