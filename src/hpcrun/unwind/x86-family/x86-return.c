// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#define _GNU_SOURCE

#include "x86-canonical.h"
#include "x86-decoder.h"
#include "x86-interval-highwatermark.h"
#include "x86-interval-arg.h"

#include "../../utilities/arch/x86-family/instruction-set.h"

/******************************************************************************
 * forward declarations
 *****************************************************************************/

static bool plt_is_next(char *ins);


/******************************************************************************
 * interface operations
 *****************************************************************************/

unwind_interval *
process_return(xed_decoded_inst_t *xptr, bool irdebug, interval_arg_t *iarg)
{
  unwind_interval *next = iarg->current;

  if (UWI_RECIPE(iarg->current)->ra_status == RA_SP_RELATIVE) {
    int offset = UWI_RECIPE(iarg->current)->reg.sp_ra_pos;
    if (offset != 0) {
      unwind_interval *u = iarg->restored_canonical;
      do {
        // fix offset
        x86recipe_t *xr = UWI_RECIPE(u);
        xr->reg.sp_ra_pos -= offset;
        xr->reg.sp_bp_pos -= offset;
      } while (u != iarg->current && (u = UWI_NEXT(u)));
    }
    if (UWI_RECIPE(iarg->current)->reg.bp_status == BP_HOSED) {
      // invariant: when we reach a return, if the BP was overwritten, it
      // should have been restored. this must be incorrect. let's reset
      // the bp status for all intervals leading up to this one since
      // the last canonical restore.
      unwind_interval *start = iarg->restored_canonical;
      unwind_interval *u = start;
      do {
        x86recipe_t *xr = UWI_RECIPE(u);
        if (xr->reg.bp_status != BP_HOSED) {
          start = NULL;
        } else if (start == NULL)
          start = u;
      } while (u != iarg->current && (u = UWI_NEXT(u)));
      u = start;
      do {
        x86recipe_t *xr = UWI_RECIPE(u);
        xr->reg.bp_status = BP_UNCHANGED;
      } while (u != iarg->current && (u = UWI_NEXT(u)));
    }
  }
  if (UWI_RECIPE(iarg->current)->reg.bp_status == BP_SAVED) {
     suspicious_interval(iarg->ins);
  }
  if ((void*)nextInsn(iarg, xptr) < iarg->end) {
    //-------------------------------------------------------------------------
    // the return is not the last instruction in the interval;
    // set up an interval for code after the return
    //-------------------------------------------------------------------------
    if (plt_is_next(nextInsn(iarg, xptr))) {
      //-------------------------------------------------------------------------
      // the code following the return is a program linkage table. each entry in
      // the program linkage table should be invoked with the return address at
      // top of the stack. this is exactly what the interval containing this
      // return instruction looks like. set the current interval as the
      // "canonical interval" to be restored after then jump at the end of each
      // entry in the PLT.
      //-------------------------------------------------------------------------
      iarg->canonical_interval = iarg->current;
    }
    else {
      reset_to_canonical_interval(xptr, &next, irdebug, iarg);
    }
  }
  return next;
}


/******************************************************************************
 * private operations
 *****************************************************************************/

static bool
plt_is_next(char *ins)
{

  // Assumes: 'ins' is pointing at the instruction from which
  // lookahead is to occur (i.e, the instruction prior to the first
  // lookahead).

  xed_state_t *xed_settings = &(x86_decoder_settings.xed_settings);

  xed_error_enum_t xed_err;
  xed_decoded_inst_t xedd;
  xed_decoded_inst_t *xptr = &xedd;
  char *val_pushed = NULL;
  char *push_succ_addr = NULL;
  char *jmp_target = NULL;

  // skip optional padding if there appears to be any
  while ((((long) ins) & 0x11) && (*ins == 0x0)) ins++;

  // -------------------------------------------------------
  // requirement 1: push of displacement relative to IP
  // -------------------------------------------------------
  xed_decoded_inst_zero_set_mode(xptr, xed_settings);
  xed_err = xed_decode(xptr, (uint8_t*) ins, 15);
  if (xed_err != XED_ERROR_NONE) {
    return false;
  }

  if (iclass_eq(xptr, XED_ICLASS_PUSH)) {
    if (xed_decoded_inst_number_of_memory_operands(xptr) == 2) {
      const xed_inst_t* xi = xed_decoded_inst_inst(xptr);
      const xed_operand_t* op0 = xed_inst_operand(xi, 0);
      if ((xed_operand_name(op0) == XED_OPERAND_MEM0) &&
        x86_isReg_IP(xed_decoded_inst_get_base_reg(xptr, 0))) {
        int64_t offset = xed_decoded_inst_get_memory_displacement(xptr, 0);
        push_succ_addr = ins + xed_decoded_inst_get_length(xptr);
        val_pushed = push_succ_addr + offset;
      }
    }
  }

  if (val_pushed == NULL) {
    // push of proper type not recognized
    return false;
  }

  // -------------------------------------------------------
  // requirement 2: jump target affects stack
  // -------------------------------------------------------
  xed_decoded_inst_zero_set_mode(xptr, xed_settings);
  xed_err = xed_decode(xptr, (uint8_t*) push_succ_addr, 15);
  if (xed_err != XED_ERROR_NONE) {
    return false;
  }

  if (iclass_eq(xptr, XED_ICLASS_JMP) ||
      iclass_eq(xptr, XED_ICLASS_JMP_FAR)) {
    if (xed_decoded_inst_number_of_memory_operands(xptr) == 1) {

      const xed_inst_t *xi = xed_decoded_inst_inst(xptr);
      const xed_operand_t *op0 =  xed_inst_operand(xi,0);
      if ((xed_operand_name(op0) == XED_OPERAND_MEM0) &&
         x86_isReg_IP(xed_decoded_inst_get_base_reg(xptr, 0))) {
         long long offset = xed_decoded_inst_get_memory_displacement(xptr,0);
         jmp_target = push_succ_addr + xed_decoded_inst_get_length(xptr) + offset;
      }
    }
  }

  if (jmp_target == NULL) {
    // jump of proper type not recognized
    return false;
  }

  //
  // FIXME: Does the 8 need to be sizeof(void*) ?????
  //
  if ((jmp_target - val_pushed) == 8){
    return true;
  }

  return false;
}
