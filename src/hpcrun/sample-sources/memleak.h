// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//
// MEMLEAK sample source public interface:
//
//  The allocate/free wrappers for the MEMLEAK sample source are not
//  housed with the rest of the MEMLEAK code. They will be separate
//  so that they are not linked into all executables.
//
//  To avoid exposing the details of the MEMLEAK handler via global variables,
//  the following procedural interfaces are provided.
//
//

#ifndef sample_source_memleak_h
#define sample_source_memleak_h

/******************************************************************************
 * local includes
 *****************************************************************************/

#include "../cct/cct.h"

/******************************************************************************
 * interface operations
 *****************************************************************************/

int hpcrun_memleak_alloc_id();
int hpcrun_memleak_active();
void hpcrun_alloc_inc(cct_node_t* node, int incr);
void hpcrun_free_inc(cct_node_t* node, int incr);

#endif // sample_source_memleak_h
