// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef HPCRUN_OMPT_OMPT_GCC4_X86_H
#define HPCRUN_OMPT_OMPT_GCC4_X86_H

//******************************************************************************
// local includes
//******************************************************************************

#include "../unwind/x86-family/x86-decoder.h"



//******************************************************************************
// macros
//******************************************************************************

#define X86_CALL_NBYTES 5 // the number of bytes in an x86 call instruction



//******************************************************************************
// operations
//******************************************************************************

static inline uint64_t
length_of_call_instruction
(
  void
)
{
  return X86_CALL_NBYTES;
}


// given an instruction pointer, find the next call instruction. return the
// difference in bytes between the end of that call instruction and the
// initial instruction pointer
static inline int
offset_to_pc_after_next_call
(
  void *ip
)
{
  xed_decoded_inst_t xedd;
  xed_decoded_inst_t *xptr = &xedd;
  xed_error_enum_t xed_error;

  uint8_t *ins = ip;

  xed_decoded_inst_zero_set_mode(xptr, &x86_decoder_settings.xed_settings);

  // continue until success or failure
  for (;;) {
    xed_decoded_inst_zero_keep_mode(xptr);
    xed_error = xed_decode(xptr, ins, 15);

    if (xed_error != XED_ERROR_NONE) {
        // on a failure, perform no adjustment.
        return 0;
    }

    // advance ins to point at the next instruction
    ins += xed_decoded_inst_get_length(xptr);

    xed_iclass_enum_t xiclass = xed_decoded_inst_get_iclass(xptr);

    switch(xiclass) {
    case XED_ICLASS_CALL_FAR:
    case XED_ICLASS_CALL_NEAR:
       return ins - (uint8_t *) ip;
    default:
       break;
    }
  }
}

#endif  // HPCRUN_OMPT_OMPT_GCC4_X86_H
