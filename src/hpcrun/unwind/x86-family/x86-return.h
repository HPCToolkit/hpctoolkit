// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef x86_return_h
#define x86_return_h

/******************************************************************************
 * include files
 *****************************************************************************/
#include "x86-unwind-analysis.h"
#include "x86-interval-arg.h"


/******************************************************************************
 * interface operations
 *****************************************************************************/

unwind_interval *
process_return(xed_decoded_inst_t *xptr, bool irdebug, interval_arg_t *iarg);

#endif
