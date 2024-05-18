// SPDX-FileCopyrightText: 2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#ifndef cuda_correlation_id_map_H
#define cuda_correlation_id_map_H

//*****************************************************************************
// system includes
//*****************************************************************************

#include <stdint.h>
#include <stdbool.h>



//*****************************************************************************
// type definitions
//*****************************************************************************

typedef struct cuda_correlation_id_map_entry_t cuda_correlation_id_map_entry_t;



//*****************************************************************************
// interface operations
//*****************************************************************************

cuda_correlation_id_map_entry_t *
cuda_correlation_id_map_lookup
(
 uint64_t gpu_correlation_id
);


bool
cuda_correlation_id_map_insert
(
 uint64_t gpu_correlation_id,
 uint64_t host_correlation_id
);


bool
cuda_correlation_id_map_delete
(
 uint64_t gpu_correlation_id
);


uint64_t
cuda_correlation_id_map_entry_get_host_id
(
 cuda_correlation_id_map_entry_t *entry
);


bool
cuda_correlation_id_map_entry_set_total_samples
(
 cuda_correlation_id_map_entry_t *entry,
 uint32_t total_samples
);


bool
cuda_correlation_id_map_entry_increment_samples
(
 cuda_correlation_id_map_entry_t *entry,
 uint32_t num_samples
);

#endif
