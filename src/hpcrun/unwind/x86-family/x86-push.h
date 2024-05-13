// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef x86_push_h
#define x86_push_h

/******************************************************************************
 * include files
 *****************************************************************************/

#include "x86-interval-highwatermark.h"
#include "x86-interval-arg.h"

/******************************************************************************
 * interface operations
 *****************************************************************************/

unwind_interval *
process_push(xed_decoded_inst_t *xptr, const xed_inst_t *xi, interval_arg_t *iarg);

unwind_interval *
process_pop(xed_decoded_inst_t *xptr, const xed_inst_t *xi, interval_arg_t *iarg);

#endif
