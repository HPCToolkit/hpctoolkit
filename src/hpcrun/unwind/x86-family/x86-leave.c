// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#define _GNU_SOURCE

#include "x86-interval-highwatermark.h"
#include "x86-interval-arg.h"

/******************************************************************************
 * interface operations
 *****************************************************************************/

unwind_interval *
process_leave(xed_decoded_inst_t *xptr, const xed_inst_t *xi, interval_arg_t *iarg)
{
  x86registers_t reg = { 0, 0, BP_UNCHANGED, 0, 0};
  unwind_interval *next;
  next = new_ui(nextInsn(iarg, xptr), RA_SP_RELATIVE, &reg);
  return next;
}
