// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef X86_INTERVAL_ARG_H
#define X86_INTERVAL_ARG_H

#include <stdbool.h>

#include "x86-interval-highwatermark.h"

#include "../../utilities/arch/x86-family/instruction-set.h"

// as extra arguments are added to the process inst routine, better
// to add fields to the structure, rather than keep changing the function signature

typedef struct interval_arg_t {
  // read only:
  void *beg;
  void *end;
  bitree_uwi_t *first;

  // read/write:
  void *ins;
  bitree_uwi_t *current;
  bool bp_just_pushed;
  highwatermark_t highwatermark;
  bitree_uwi_t *canonical_interval;
  unwind_interval *restored_canonical;
  bool bp_frames_found;
  void *rax_rbp_equivalent_at;
  void *return_addr; // A place to store void * return values.
  bool sp_realigned; // stack pointer was realigned by masking lower bits
} interval_arg_t;

static inline char *nextInsn(interval_arg_t *iarg, xed_decoded_inst_t *xptr) {
  return iarg->ins + xed_decoded_inst_get_length(xptr);
}

#endif // X86_INTERVAL_ARG_H
