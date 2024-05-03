// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

/******************************************************************************
 * include files
 *****************************************************************************/

#define _GNU_SOURCE

#include "x86-interval-highwatermark.h"
#include "x86-decoder.h"
#include "x86-interval-arg.h"

#include <assert.h>
#include <stdlib.h>

#include "../../messages/messages.h"
#include "../../utilities/arch/x86-family/instruction-set.h"

/******************************************************************************
 * interface operations
 *****************************************************************************/

unwind_interval *
process_push(xed_decoded_inst_t *xptr, const xed_inst_t *xi, interval_arg_t *iarg)
{
  unwind_interval *next;

  const xed_operand_t *op0 =  xed_inst_operand(xi, 0);
  xed_operand_enum_t   op0_name = xed_operand_name(op0);
  x86recipe_t *xr = UWI_RECIPE(iarg->current);
  x86registers_t reg = xr->reg;
  bp_loc bp_status = reg.bp_status;
  int size;

  switch(iclass(xptr)) {
  case XED_ICLASS_PUSH:   size = sizeof(void*); break;
  case XED_ICLASS_PUSHFQ: size = 8; break;
  case XED_ICLASS_PUSHFD: size = 4; break;
  case XED_ICLASS_PUSHF:  size = 2; break;
  default:
    assert(false && "Invalid XED instruction class");
    hpcrun_terminate();
  }

  reg.sp_ra_pos += size;
  reg.sp_bp_pos += size;
  if (op0_name == XED_OPERAND_REG0) {
    xed_reg_enum_t regname = xed_decoded_inst_get_reg(xptr, op0_name);
    if (x86_isReg_BP(regname) && bp_status == BP_UNCHANGED) {
      reg.bp_status = BP_SAVED;
      reg.sp_bp_pos = 0;
    }
  }

  next = new_ui(nextInsn(iarg, xptr), xr->ra_status, &reg);

  return next;
}


unwind_interval *
process_pop(xed_decoded_inst_t *xptr, const xed_inst_t *xi, interval_arg_t *iarg)
{
  unwind_interval *next;

  const xed_operand_t *op0 =  xed_inst_operand(xi, 0);
  xed_operand_enum_t   op0_name = xed_operand_name(op0);
  x86recipe_t *xr = UWI_RECIPE(iarg->current);
  x86registers_t reg = xr->reg;
  int size;

  switch(iclass(xptr)) {
  case XED_ICLASS_POP:   size = -((int)sizeof(void*));  break;
  case XED_ICLASS_POPFQ: size = -8;  break;
  case XED_ICLASS_POPFD: size = -4;  break;
  case XED_ICLASS_POPF:  size = -2;  break;
  default:
    assert(false && "Invalid XED instruction class");
    hpcrun_terminate();
  }

  reg.sp_ra_pos += size;
  reg.sp_bp_pos += size;
  if (op0_name == XED_OPERAND_REG0) {
    xed_reg_enum_t regname = xed_decoded_inst_get_reg(xptr, op0_name);
    if (x86_isReg_BP(regname)) reg.bp_status = BP_UNCHANGED;
  }

  next = new_ui(nextInsn(iarg, xptr), xr->ra_status, &reg);
  return next;
}
