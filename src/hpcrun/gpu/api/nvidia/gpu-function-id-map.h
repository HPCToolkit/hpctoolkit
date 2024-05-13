// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef gpu_function_id_map_h
#define gpu_function_id_map_h

/******************************************************************************
 * system includes
 *****************************************************************************/

#include <stdint.h>

/******************************************************************************
 * local includes
 *****************************************************************************/

#include "../../../cct/cct.h"
#include "../../../utilities/ip-normalized.h"

/******************************************************************************
 * type definitions
 *****************************************************************************/

typedef struct gpu_function_id_map_entry_t gpu_function_id_map_entry_t;

/******************************************************************************
 * interface operations
 *****************************************************************************/

gpu_function_id_map_entry_t *
gpu_function_id_map_lookup
(
 uint64_t function_id
);


void
gpu_function_id_map_insert
(
 uint64_t function_id,
 ip_normalized_t pc
);


ip_normalized_t
gpu_function_id_map_entry_pc_get
(
 gpu_function_id_map_entry_t *entry
);


void
gpu_function_id_map_delete
(
 uint64_t function_id
);

#endif
