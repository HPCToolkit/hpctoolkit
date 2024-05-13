// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef gpu_event_id_map_h
#define gpu_event_id_map_h



//*****************************************************************************
// system includes
//*****************************************************************************

#include <stdint.h>



//*****************************************************************************
// type declarations
//*****************************************************************************

typedef struct gpu_event_id_map_entry_value_t {
  uint32_t context_id;
  uint32_t stream_id;
} gpu_event_id_map_entry_value_t;



//*****************************************************************************
// interface operations
//*****************************************************************************

gpu_event_id_map_entry_value_t *
gpu_event_id_map_lookup
(
 uint32_t event_id
);


void
gpu_event_id_map_insert
(
 uint32_t event_id,
 uint32_t context_id,
 uint32_t stream_id
);


void
gpu_event_id_map_delete
(
 uint32_t event_id
);


#endif
