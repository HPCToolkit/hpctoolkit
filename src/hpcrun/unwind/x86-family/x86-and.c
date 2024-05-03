// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//************************* System Include Files ****************************

//*************************** User Include Files ****************************

#define _GNU_SOURCE

#include "x86-decoder.h"
#include "x86-interval-arg.h"

#include "../../utilities/arch/x86-family/instruction-set.h"

//***************************************************************************

unwind_interval *
process_and(xed_decoded_inst_t *xptr, const xed_inst_t *xi, interval_arg_t *iarg)
{
  unwind_interval *next = iarg->current;
  const xed_operand_t* op0 = xed_inst_operand(xi,0);
  xed_operand_enum_t   op0_name = xed_operand_name(op0);

  if (op0_name == XED_OPERAND_REG0) {
    xed_reg_enum_t reg0 = xed_decoded_inst_get_reg(xptr, op0_name);
    if (x86_isReg_SP(reg0)) {
      x86recipe_t *xr = UWI_RECIPE(iarg->current);
      if (xr->reg.bp_status != BP_UNCHANGED) {
        //----------------------------------------------------------------------
        // we are adjusting the stack pointer via 'and' instruction
        //----------------------------------------------------------------------
        next = new_ui(nextInsn(iarg, xptr), RA_BP_FRAME, &xr->reg);
      } else {
        // remember that SP was adjusted by masking bits
        iarg->sp_realigned = true;
      }
    }
  }
  return next;
}
