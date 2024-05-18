// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef _hpctoolkit_blame_map_h_
#define _hpctoolkit_blame_map_h_

//******************************************************************************
//
// map for recording directed blame for locks, critical sections, ...
//
//******************************************************************************


/******************************************************************************
 * system includes
 *****************************************************************************/

#include <stdint.h>

//
// (abstract) data type definition
//
typedef union blame_entry_t blame_entry_t;

/***************************************************************************
 * interface operations
 ***************************************************************************/

blame_entry_t* blame_map_new(void);
void blame_map_init(blame_entry_t* table);
void blame_map_add_blame(blame_entry_t* table,
                         uint64_t obj, uint32_t metric_value);
uint64_t blame_map_get_blame(blame_entry_t* table, uint64_t obj);

#endif // _hpctoolkit_blame_map_h_
