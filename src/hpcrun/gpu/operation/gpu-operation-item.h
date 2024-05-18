// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef gpu_operation_item_h
#define gpu_operation_item_h



//******************************************************************************
// system includes
//******************************************************************************

#include <stdint.h>



//******************************************************************************
// local includes
//******************************************************************************

#include "../../../common/lean/stacks.h"
#include "../../utilities/ip-normalized.h"

#include "../activity/gpu-activity-channel.h"



//******************************************************************************
// forward declarations
//******************************************************************************

typedef struct gpu_operation_channel_t gpu_operation_channel_t;



//******************************************************************************
// type declarations
//******************************************************************************

typedef struct gpu_operation_item_t {
  s_element_t next;
  gpu_activity_channel_t *channel;
  gpu_activity_t activity;
  atomic_int *pending_operations;
  atomic_bool *flush;
} gpu_operation_item_t;



//******************************************************************************
// interface functions
//******************************************************************************

/**
 * Logs \p context and \p operation_item
*/
void
gpu_operation_item_dump
(
 const gpu_operation_item_t *item,
 const char *context
);



#endif
