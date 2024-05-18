// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//************************* System Include Files ****************************

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include <ucontext.h>

//*************************** User Include Files ****************************

#include "x86-decoder.h"
#include "../common/unwind.h"

//*************************** Forward Declarations **************************


//***************************************************************************
// interface functions
//***************************************************************************

static void*
actual_get_branch_target(void *ins, xed_decoded_inst_t *xptr,
                   xed_operand_values_t *vals)
{
#ifdef XED_DISPLACEMENT_INT64
  int64_t offset = xed_operand_values_get_branch_displacement_int64(vals);
#else
  int32_t offset = xed_operand_values_get_branch_displacement_int32(vals);
#endif

  void *end_of_call_inst = ins + xed_decoded_inst_get_length(xptr);
  void *target = end_of_call_inst + offset;
  return target;
}


void *
x86_get_branch_target(void *ins, xed_decoded_inst_t *xptr)
{
  const xed_inst_t *xi = xed_decoded_inst_inst(xptr);
  const xed_operand_t *op0 =  xed_inst_operand(xi, 0);
  xed_operand_enum_t   op0_name = xed_operand_name(op0);
  xed_operand_type_enum_t op0_type = xed_operand_type(op0);

  if (op0_name == XED_OPERAND_RELBR &&
      op0_type == XED_OPERAND_TYPE_IMM_CONST) {
    xed_operand_values_t *vals = xed_decoded_inst_operands(xptr);

    if (xed_operand_values_has_branch_displacement(vals)) {
      void *vaddr = actual_get_branch_target(ins, xptr, vals);
      return vaddr;
    }
  }
  return NULL;
}
