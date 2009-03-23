// -*-Mode: C++;-*- // technically C99
// $Id$

#include "x86-interval-highwatermark.h"
#include "x86-decoder.h"

unwind_interval *
process_addsub(char *ins, xed_decoded_inst_t *xptr, const xed_inst_t *xi, 
	       unwind_interval *current, highwatermark_t *highwatermark, 
	       unwind_interval **canonical_interval,
	       bool *bp_frames_found)
{
  unwind_interval *next = current;
  const xed_operand_t* op0 = xed_inst_operand(xi,0);
  const xed_operand_t* op1 = xed_inst_operand(xi,1);
  xed_operand_enum_t   op0_name = xed_operand_name(op0);

  if (op0_name == XED_OPERAND_REG0) {
    xed_reg_enum_t reg0 = xed_decoded_inst_get_reg(xptr, op0_name);
    if (is_reg_sp(reg0)) {
      //-----------------------------------------------------------------------
      // we are adjusting the stack pointer
      //-----------------------------------------------------------------------

      if (xed_operand_name(op1) == XED_OPERAND_IMM0) {
	int sign = (iclass_eq(xptr, XED_ICLASS_ADD)) ? -1 : 1;
	long immedv = sign * xed_decoded_inst_get_signed_immediate(xptr);
	ra_loc istatus = current->ra_status;
	if ((istatus == RA_STD_FRAME) && (immedv > 0) &&
	    (highwatermark->state & HW_SP_DECREMENTED)) {
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
	next = new_ui(ins + xed_decoded_inst_get_length(xptr), 
		      istatus, current->sp_ra_pos + immedv, current->bp_ra_pos, 
		      current->bp_status, current->sp_bp_pos + immedv, 
		      current->bp_bp_pos, current);

	if (immedv > 0) {
	  if (HW_TEST_STATE(highwatermark->state, 0, HW_SP_DECREMENTED)) {
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
	    highwatermark->uwi = next;
	    highwatermark->succ_inst_ptr = 
	      ins + xed_decoded_inst_get_length(xptr);
	    highwatermark->state = 
	      HW_NEW_STATE(highwatermark->state, HW_SP_DECREMENTED);
	    *canonical_interval = next;
	  }
	}
      } else {
	if (current->ra_status != RA_BP_FRAME){
	  //-------------------------------------------------------------------
	  // no immediate in add/subtract from stack pointer; switch to
	  // BP_FRAME
	  //
	  // 9 December 2007 -- John Mellor-Crummey
	  //-------------------------------------------------------------------
	  next = new_ui(ins + xed_decoded_inst_get_length(xptr), RA_BP_FRAME, 
			current->sp_ra_pos, current->bp_ra_pos, 
			current->bp_status, current->sp_bp_pos, 
			current->bp_bp_pos, current);
	  *bp_frames_found = true;
	}
      }
    }
  }
  return next;
}
