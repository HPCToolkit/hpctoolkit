// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef kernel_context_map_h
#define kernel_context_map_h


//*****************************************************************************
// system includes
//*****************************************************************************

#include <stdint.h>



//*****************************************************************************
// type definitions
//*****************************************************************************

typedef struct kernel_context_map_entry_t kernel_context_map_entry_t;



//*****************************************************************************
// interface operations
//*****************************************************************************

kernel_context_map_entry_t *
kernel_context_map_lookup
(
 uint64_t
);


void
kernel_context_map_insert
(
 uint64_t,
 uint64_t
);


void
kernel_context_map_delete
(
 uint64_t
);


uint64_t
kernel_context_map_entry_kernel_id_get
(
 kernel_context_map_entry_t *entry
);


uint64_t
kernel_context_map_entry_context_id_get
(
 kernel_context_map_entry_t *entry
);

#endif  // kernel_context_map_h
