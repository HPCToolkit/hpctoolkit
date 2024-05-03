// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef device_map_h
#define device_map_h


//*****************************************************************************
// system includes
//*****************************************************************************

#include <stdbool.h>
#include <stdint.h>



//*****************************************************************************
// type definitions
//*****************************************************************************

typedef struct device_map_entry_t device_map_entry_t;



//*****************************************************************************
// interface operations
//*****************************************************************************

device_map_entry_t *
device_map_lookup
(
 uint64_t
);


bool
device_map_insert
(
 uint64_t
);


void
device_map_delete
(
 uint64_t
);

#endif  // device_map_h
