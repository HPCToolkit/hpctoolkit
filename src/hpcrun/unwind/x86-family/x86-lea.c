// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#define _GNU_SOURCE

#include "x86-interval-highwatermark.h"
#include "x86-decoder.h"
#include "x86-lea.h"
#include "x86-interval-arg.h"

#include "../../utilities/arch/x86-family/instruction-set.h"

unwind_interval *
process_lea(xed_decoded_inst_t *xptr, const xed_inst_t *xi, interval_arg_t *iarg)
{
  highwatermark_t *hw_tmp = &(iarg->highwatermark);
  unwind_interval *next = iarg->current;
  const xed_operand_t *op0 =  xed_inst_operand(xi, 0);
  xed_operand_enum_t   op0_name = xed_operand_name(op0);

  if (op0_name == XED_OPERAND_REG0) {
    x86recipe_t *xr = UWI_RECIPE(next);
    x86registers_t reg = xr->reg;
    xed_reg_enum_t regname = xed_decoded_inst_get_reg(xptr, op0_name);
    char *next_ins = nextInsn(iarg, xptr);
    if (x86_isReg_BP(regname)) {
      //=======================================================================
      // action: clobbering the base pointer; begin a new SP_RELATIVE interval
      // note: we don't check that BP is BP_SAVED; we might have to
      //=======================================================================
      reg.bp_status = BP_HOSED;
      next = new_ui(next_ins, RA_SP_RELATIVE, &reg);
      if (HW_TEST_STATE(hw_tmp->state, HW_BP_SAVED, HW_BP_OVERWRITTEN) &&
          (UWI_RECIPE(hw_tmp->uwi)->reg.sp_ra_pos == xr->reg.sp_ra_pos)) {
        hw_tmp->uwi = next;
        hw_tmp->state = HW_NEW_STATE(hw_tmp->state, HW_BP_OVERWRITTEN);
      }
    } else if (x86_isReg_SP(regname)) {
      if (xr->ra_status == RA_SP_RELATIVE || xr->ra_status == RA_STD_FRAME) {
        unsigned int memops = xed_decoded_inst_number_of_memory_operands(xptr);
        if (memops > 0) {
          int mem_op_index = 0;

          xed_reg_enum_t basereg =
            xed_decoded_inst_get_base_reg(xptr, mem_op_index);

          if (x86_isReg_SP(basereg)) {
            //==================================================================
            // the LEA instruction adjusts SP with a displacement.
            // begin a new interval where sp_ra_pos is adjusted by the
            // displacement.
            //==================================================================
            xed_int64_t disp =
              xed_decoded_inst_get_memory_displacement(xptr, mem_op_index);
            reg.sp_ra_pos -= disp;
            reg.sp_bp_pos -= disp;
            next = new_ui(next_ins, xr->ra_status, &reg);

            if (disp < 0) {
              if (HW_TEST_STATE(hw_tmp->state, 0, HW_SP_DECREMENTED)) {
                //--------------------------------------------------------------
                // set the highwatermark and canonical interval upon seeing
                // the FIRST subtract (using lea with negative displacement)
                // from SP; take no action on subsequent subtracts.
                // test case: pthread_cond_wait@@GLIBC_2.3.2 in
                // 3.10.0-327.el7.centos.mpsp_1.3.1.45.x86_64
                //
                // 5 November 2016 -- John Mellor-Crummey
                //--------------------------------------------------------------
                hw_tmp->uwi = next;
                hw_tmp->succ_inst_ptr = next_ins;
                hw_tmp->state = HW_NEW_STATE(hw_tmp->state, HW_SP_DECREMENTED);
                iarg->canonical_interval = next;
              }
            }
          }
        }
      }
    }
  }
  return next;
}
