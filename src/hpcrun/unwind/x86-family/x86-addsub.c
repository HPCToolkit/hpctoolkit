// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//************************* System Include Files ****************************

//*************************** User Include Files ****************************

#define _GNU_SOURCE

#include "x86-interval-highwatermark.h"
#include "x86-decoder.h"
#include "x86-interval-arg.h"

#include "../../utilities/arch/x86-family/instruction-set.h"

//***************************************************************************

unwind_interval *
process_addsub(xed_decoded_inst_t *xptr, const xed_inst_t *xi, interval_arg_t *iarg)
{
  highwatermark_t *hw_tmp = &(iarg->highwatermark);

  unwind_interval *next = iarg->current;
  const xed_operand_t* op0 = xed_inst_operand(xi,0);
  const xed_operand_t* op1 = xed_inst_operand(xi,1);
  xed_operand_enum_t   op0_name = xed_operand_name(op0);
  x86recipe_t *xr = UWI_RECIPE(iarg->current);
  x86registers_t reg = xr->reg;
  ra_loc istatus = xr->ra_status;

  if (op0_name == XED_OPERAND_REG0) {
    xed_reg_enum_t reg0 = xed_decoded_inst_get_reg(xptr, op0_name);
    if (x86_isReg_SP(reg0)) {
      //-----------------------------------------------------------------------
      // we are adjusting the stack pointer
      //-----------------------------------------------------------------------

      if (xed_operand_name(op1) == XED_OPERAND_IMM0) {
        int sign = (iclass_eq(xptr, XED_ICLASS_ADD)) ? -1 : 1;
        long immedv = sign * xed_decoded_inst_get_signed_immediate(xptr);
        if ((istatus == RA_STD_FRAME) && (immedv > 0) &&
            (hw_tmp->state & HW_SP_DECREMENTED)) {
          //-------------------------------------------------------------------
          // if we are in a standard frame and we see a second subtract,
          // it is time to convert interval to a BP frame to minimize
          // the chance we get the wrong offset for the return address
          // in a routine that manipulates SP frequently (as in
          // leapfrog_mod_leapfrog_ in the SPEC CPU2006 benchmark
          // 459.GemsFDTD, when compiled with PGI 7.0.3 with high levels
          // of optimization).
          //
          // 9 December 2007 -- John Mellor-Crummey
          //-------------------------------------------------------------------
        }
        reg.sp_ra_pos += immedv;
        reg.sp_bp_pos += immedv;
        next = new_ui(nextInsn(iarg, xptr), istatus, &reg);

        if (immedv > 0) {
          if (HW_TEST_STATE(hw_tmp->state, 0, HW_SP_DECREMENTED)) {
            //-----------------------------------------------------------------
            // set the highwatermark and canonical interval upon seeing
            // the FIRST subtract from SP; take no action on subsequent
            // subtracts.
            //
            // test case: main in SPEC CPU2006 benchmark 470.lbm
            // contains multiple subtracts from SP when compiled with
            // PGI 7.0.3 with high levels of optimization. the first
            // subtract from SP is to set up the frame; subsequent ones
            // are to reserve space for arguments passed to functions.
            //
            // 9 December 2007 -- John Mellor-Crummey
            //-----------------------------------------------------------------
            hw_tmp->uwi = next;
            hw_tmp->succ_inst_ptr = nextInsn(iarg, xptr);
            hw_tmp->state = HW_NEW_STATE(hw_tmp->state, HW_SP_DECREMENTED);
            iarg->canonical_interval = next;
          }
        }
      } else {
        if (istatus != RA_BP_FRAME){
          //-------------------------------------------------------------------
          // no immediate in add/subtract from stack pointer; switch to
          // BP_FRAME
          //
          // 9 December 2007 -- John Mellor-Crummey
          //-------------------------------------------------------------------
          next = new_ui(nextInsn(iarg, xptr), RA_BP_FRAME, &reg);
          iarg->bp_frames_found = true;
        }
      }
    }
  }
  return next;
}
