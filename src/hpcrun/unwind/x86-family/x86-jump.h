// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef x86_jump_h
#define x86_jump_h

/******************************************************************************
 * includes
 *****************************************************************************/

#include "x86-unwind-analysis.h"

/******************************************************************************
 * interface operations
 *****************************************************************************/

unwind_interval *
process_unconditional_branch(xed_decoded_inst_t *xptr, bool irdebug,
        interval_arg_t *iarg);

unwind_interval *
process_conditional_branch(xed_decoded_inst_t *xptr, interval_arg_t *iarg);

#endif
