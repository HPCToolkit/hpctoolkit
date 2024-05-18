// SPDX-FileCopyrightText: 2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#ifndef GPU_OP_CCTS_MAP_H
#define GPU_OP_CCTS_MAP_H

//*****************************************************************************
// system includes
//*****************************************************************************

#include <stdint.h>
#include <stdbool.h>



//******************************************************************************
// local includes
//******************************************************************************

#include "gpu-op-placeholders.h"



//*****************************************************************************
// type definitions
//*****************************************************************************

typedef struct gpu_op_ccts_map_entry_value_t {
  gpu_op_ccts_t gpu_op_ccts;
  uint64_t cpu_submit_time;
} gpu_op_ccts_map_entry_value_t;

typedef struct gpu_op_ccts_map_entry_t gpu_op_ccts_map_entry_t;



//*****************************************************************************
// interface operations
//*****************************************************************************

gpu_op_ccts_map_entry_value_t *
gpu_op_ccts_map_lookup
(
 uint64_t gpu_correlation_id
);


bool
gpu_op_ccts_map_insert
(
 uint64_t gpu_correlation_id,
 gpu_op_ccts_map_entry_value_t value
);


bool
gpu_op_ccts_map_delete
(
 uint64_t gpu_correlation_id
);


#endif
