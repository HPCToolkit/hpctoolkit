// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef HPCRUN_OMPT_OMPT_GCC4_PPC64_H
#define HPCRUN_OMPT_OMPT_GCC4_PPC64_H

//******************************************************************************
// local includes
//******************************************************************************

#include "../utilities/arch/ppc64/instruction-set.h"



//******************************************************************************
// macros
//******************************************************************************

#define PPC64_CALL_NBYTES 4 // the number of bytes in a PPC64 branch-and-link



//******************************************************************************
// operations
//******************************************************************************

static inline uint64_t
length_of_call_instruction
(
  void
)
{
  return PPC64_CALL_NBYTES;
}


// return true iff the instruction is a PPC64 branch and link
static inline bool
isInsn_BL
(
  uint32_t insn
)
{
  return (insn & PPC_OP_I_MASK) == PPC_OP_BL;
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
  uint32_t *insn = (uint32_t *) ip;

  while (!isInsn_BL(*insn++));

  return ((char *) insn) - ((char *) ip);
}

#endif  // HPCRUN_OMPT_OMPT_GCC4_PPC64_H
