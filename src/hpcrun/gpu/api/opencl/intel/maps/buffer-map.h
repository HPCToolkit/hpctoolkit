// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef buffer_map_h
#define buffer_map_h


//*****************************************************************************
// system includes
//*****************************************************************************

#include <stdint.h>



//*****************************************************************************
// type definitions
//*****************************************************************************

typedef struct buffer_map_entry_t buffer_map_entry_t;



//*****************************************************************************
// interface operations
//*****************************************************************************

buffer_map_entry_t *
buffer_map_lookup
(
 uint64_t
);


buffer_map_entry_t*
buffer_map_update
(
 uint64_t,
 int,
 int
);


void
buffer_map_delete
(
 uint64_t
);


uint64_t
buffer_map_entry_buffer_id_get
(
 buffer_map_entry_t *entry
);


int
buffer_map_entry_D2H_count_get
(
 buffer_map_entry_t *entry
);

#endif  // buffer_map_h
