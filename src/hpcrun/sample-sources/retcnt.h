// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//
// RETCNT sample source public interface:
//  The handler for the RETCNT sample source is not housed with the
//  rest of the RETCNT code. (retcnt handler = trampoline.c).
//  To avoid exposing the details of the RETCNT handler via global variables,
//  the following procedural interface is provided.
//
//

#ifndef sample_source_retcnt_h
#define sample_source_retcnt_h

/******************************************************************************
 * local includes
 *****************************************************************************/

#include "../cct/cct.h"

/******************************************************************************
 * interface operations
 *****************************************************************************/

void hpcrun_retcnt_inc(cct_node_t* node, int incr);

#endif // sample_source_retcnt_h
