// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef x86_call_h
#define x86_call_h

#include "x86-unwind-interval.h"
#include "x86-unwind-analysis.h"
#include "x86-interval-arg.h"

/******************************************************************************
 * interface operations
 *****************************************************************************/

unwind_interval*
process_call(xed_decoded_inst_t *xptr, const xed_inst_t *xi, interval_arg_t *iarg);

#endif
