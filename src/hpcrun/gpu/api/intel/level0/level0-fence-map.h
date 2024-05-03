// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef level0_fence_map_h
#define level0_fence_map_h

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
} level0_fence_data_t;

//******************************************************************************
// interface operations
//******************************************************************************

void
level0_fence_map_insert
(
  ze_fence_handle_t hFence,
  uint32_t numCommandLists,
  ze_command_list_handle_t* phCommandLists
);

level0_fence_data_t*
level0_fence_map_lookup
(
  ze_fence_handle_t hFence
);

void
level0_fence_map_delete
(
  ze_fence_handle_t hFence
);

#endif
