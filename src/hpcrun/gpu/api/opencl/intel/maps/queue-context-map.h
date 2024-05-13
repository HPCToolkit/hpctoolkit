// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef queue_context_map_h
#define queue_context_map_h


//*****************************************************************************
// system includes
//*****************************************************************************

#include <stdint.h>



//*****************************************************************************
// type definitions
//*****************************************************************************

typedef struct queue_context_map_entry_t queue_context_map_entry_t;

//*****************************************************************************
// interface operations
//*****************************************************************************

queue_context_map_entry_t *
queue_context_map_lookup
(
 uint64_t
);


void
queue_context_map_insert
(
 uint64_t,
 uint64_t
);


void
queue_context_map_delete
(
 uint64_t
);


uint64_t
queue_context_map_entry_queue_id_get
(
 queue_context_map_entry_t *entry
);


uint64_t
queue_context_map_entry_context_id_get
(
 queue_context_map_entry_t *
);


#endif  // queue_context_map_h
