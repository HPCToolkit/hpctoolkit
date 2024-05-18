// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

/******************************************************************************
 * include files
 *****************************************************************************/

#define _GNU_SOURCE

#include "x86-interval-highwatermark.h"
#include "x86-interval-arg.h"

#include "../../messages/messages.h"

/******************************************************************************
 * interface operations
 *****************************************************************************/

unwind_interval *
process_enter(xed_decoded_inst_t *xptr, const xed_inst_t *xi, interval_arg_t *iarg)
{
  unsigned int i;
  unwind_interval *next;

  highwatermark_t *hw_tmp = &(iarg->highwatermark);

  long offset = 8;

  for(i = 0; i < xed_inst_noperands(xi) ; i++) {
    const xed_operand_t *op =  xed_inst_operand(xi,i);
    switch (xed_operand_name(op)) {
    case XED_OPERAND_IMM0SIGNED:
      offset += xed_decoded_inst_get_signed_immediate(xptr);
      break;
    default:
      break;
    }
  }
  TMSG(INTV,"new interval from ENTER");
  x86registers_t reg = UWI_RECIPE(iarg->current)->reg;
  reg.sp_ra_pos += offset;
  reg.bp_ra_pos = 8;
  reg.bp_status = BP_SAVED;
  reg.sp_bp_pos += offset - 8;
  reg.bp_bp_pos = 0;

  next = new_ui(nextInsn(iarg, xptr), RA_STD_FRAME, &reg);
  hw_tmp->uwi = next;
  hw_tmp->state =
    HW_NEW_STATE(hw_tmp->state, HW_BP_SAVED |
                 HW_SP_DECREMENTED | HW_BP_OVERWRITTEN);
  return next;
}
