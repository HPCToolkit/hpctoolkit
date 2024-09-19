// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef level0_data_node_h
#define level0_data_node_h

//*****************************************************************************
// system includes
//*****************************************************************************

#include <stdint.h>

//*****************************************************************************
// local includes
//*****************************************************************************

#include "level0-api.h"

#include "../../../../cct/cct.h"
#include "../../../activity/gpu-activity-channel.h"
#ifndef __cplusplus
#include <stdatomic.h>
#else
#include <atomic>
#endif

//******************************************************************************
// type declarations
//******************************************************************************

typedef enum level0_command_type {
  LEVEL0_KERNEL,
  LEVEL0_MEMCPY
} level0_command_type_t;

typedef struct level0_kernel_entry {
  ze_kernel_handle_t kernel;
} level0_kernel_entry_t;

typedef struct level0_memcpy_entry {
  ze_memory_type_t src_type;
  ze_memory_type_t dst_type;
  size_t copy_size;
} level0_memcpy_entry_t;

typedef union level0_detail_entry {
  level0_kernel_entry_t kernel;
  level0_memcpy_entry_t memcpy;
} level0_detail_entry_t;

typedef struct level0_data_node {
  level0_command_type_t type;
  const struct hpcrun_foil_appdispatch_level0* dispatch;
  ze_event_handle_t event;
  ze_event_pool_handle_t event_pool;
  gpu_activity_channel_t *initiator_channel;
  cct_node_t *cct_node;
  cct_node_t *kernel;
  atomic_int *pending_operations;
  uint64_t submit_time;
  level0_detail_entry_t details;
  struct level0_data_node *next;
} level0_data_node_t;


//*****************************************************************************
// interface operations
//*****************************************************************************

level0_data_node_t*
level0_data_node_new
(
);

// Return a node for the linked list to the free list
void
level0_data_node_return_free_list
(
  level0_data_node_t* node
);

void
level0_data_list_free
(
 level0_data_node_t* head
);
#endif
