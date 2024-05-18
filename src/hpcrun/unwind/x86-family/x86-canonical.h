// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef x86_canonical_h
#define x86_canonical_h

#include "x86-unwind-analysis.h"
#include "x86-interval-arg.h"


/******************************************************************************
 * interface operations
 *****************************************************************************/

void
reset_to_canonical_interval(xed_decoded_inst_t *xptr, unwind_interval **next,
        bool irdebug, interval_arg_t *iarg);

#endif //x86_canonical_h
