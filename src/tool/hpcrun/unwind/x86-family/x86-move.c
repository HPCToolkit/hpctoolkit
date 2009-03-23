#include "x86-interval-highwatermark.h"
#include "x86-decoder.h"

unwind_interval *
process_move(char *ins, xed_decoded_inst_t *xptr, const xed_inst_t *xi, 
	     unwind_interval *current, highwatermark_t *highwatermark, 
	     char **rax_rbp_equivalent_at)
{
  unwind_interval *next = current;

  const xed_operand_t *op0 =  xed_inst_operand(xi, 0);
  const xed_operand_t *op1 =  xed_inst_operand(xi, 1);

  xed_operand_enum_t   op0_name = xed_operand_name(op0);
  xed_operand_enum_t   op1_name = xed_operand_name(op1);
    
  if ((op0_name == XED_OPERAND_MEM0) && (op1_name == XED_OPERAND_REG0)) { 
    //------------------------------------------------------------------------
    // storing a register to memory 
    //------------------------------------------------------------------------
    xed_reg_enum_t basereg = xed_decoded_inst_get_base_reg(xptr, 0);
    if (is_reg_sp(basereg)) {
      //----------------------------------------------------------------------
      // a memory move with SP as a base register
      //----------------------------------------------------------------------
      xed_reg_enum_t reg1 = xed_decoded_inst_get_reg(xptr, op1_name);
      if (is_reg_bp(reg1) ||  
	(is_reg_ax(reg1) && (*rax_rbp_equivalent_at == ins)))  {
	//--------------------------------------------------------------------
	// register being stored is BP (or a copy in RAX)
	//--------------------------------------------------------------------
	if (current->bp_status == BP_UNCHANGED) {
	  //==================================================================
	  // instruction: save caller's BP into the stack  
	  // action:      create a new interval with 
	  //                (1) BP status reset to BP_SAVED
	  //                (2) BP position relative to the stack pointer set 
	  //                    to the offset from SP 
	  //==================================================================
	  next = new_ui(ins + xed_decoded_inst_get_length(xptr),
			current->ra_status, current->sp_ra_pos, 
			current->bp_ra_pos,
			BP_SAVED,
			xed_decoded_inst_get_memory_displacement(xptr, 0),
			current->bp_bp_pos,
			current);
	  highwatermark->uwi = next;
	  highwatermark->state = 
	    HW_NEW_STATE(highwatermark->state, HW_BP_SAVED);
	}
      }
    }
  } else if ((op1_name == XED_OPERAND_MEM0) && (op0_name == XED_OPERAND_REG0)) { 
    //----------------------------------------------------------------------
    // loading a register from memory 
    //----------------------------------------------------------------------
    xed_reg_enum_t reg0 = xed_decoded_inst_get_reg(xptr, op0_name);
    if (is_reg_bp(reg0)) {
      //--------------------------------------------------------------------
      // register being loaded is BP
      //--------------------------------------------------------------------
      if (current->bp_status != BP_UNCHANGED) {
	int64_t offset = xed_decoded_inst_get_memory_displacement(xptr, 0);
	xed_reg_enum_t basereg = xed_decoded_inst_get_base_reg(xptr, 0); 
	if (is_reg_sp(basereg) && (offset == current->sp_bp_pos)) { 
	  //================================================================
	  // instruction: restore BP from its saved location in the stack  
	  // action:      create a new interval with BP status reset to 
	  //              BP_UNCHANGED
	  //================================================================
	  next = new_ui(ins + xed_decoded_inst_get_length(xptr),
			RA_SP_RELATIVE, current->sp_ra_pos, 
			current->bp_ra_pos, BP_UNCHANGED, current->sp_bp_pos, 
			current->bp_bp_pos, current);
	} else {
	  //================================================================
	  // instruction: BP is loaded from a memory address DIFFERENT from 
	  // its saved location in the stack
	  // action:      create a new interval with BP status reset to 
	  //              BP_HOSED
	  //================================================================
	  if (current->bp_status != BP_HOSED) {
	    next = new_ui(ins + xed_decoded_inst_get_length(xptr),
			  RA_SP_RELATIVE, current->sp_ra_pos, 
			  current->bp_ra_pos, BP_HOSED, current->sp_bp_pos,
			  current->bp_bp_pos, current);
	    if (HW_TEST_STATE(highwatermark->state, HW_BP_SAVED, 
			      HW_BP_OVERWRITTEN) && 
		(highwatermark->uwi->sp_ra_pos == next->sp_ra_pos)) {
	      highwatermark->uwi = next;
	      highwatermark->state = 
		HW_NEW_STATE(highwatermark->state, HW_BP_OVERWRITTEN);
	    }
	  }
	}
      }
    } else if (is_reg_sp(reg0)) {
      //--------------------------------------------------------------------
      // register being loaded is SP
      //--------------------------------------------------------------------
      xed_reg_enum_t basereg = xed_decoded_inst_get_base_reg(xptr, 0); 
      if (is_reg_sp(basereg)) { 
	//================================================================
	// instruction: restore SP from a saved location in the stack  
	// action:      create a new interval with SP status reset to 
	//              BP_UNCHANGED
	//================================================================
	next = new_ui(ins + xed_decoded_inst_get_length(xptr),
	 	      RA_SP_RELATIVE, 0, 0,
		      current->bp_status, current->sp_bp_pos, 
		      current->bp_bp_pos, current);
      }
    }
  } else if ((op0_name == XED_OPERAND_REG0) && (op1_name == XED_OPERAND_REG1)){
    //----------------------------------------------------------------------
    // register-to-register move 
    //----------------------------------------------------------------------
    xed_reg_enum_t reg0 = xed_decoded_inst_get_reg(xptr, op0_name);
    xed_reg_enum_t reg1 = xed_decoded_inst_get_reg(xptr, op1_name);
    if (is_reg_bp(reg1) && is_reg_sp(reg0)) {
      //====================================================================
      // instruction: restore SP from BP
      // action:      begin a new SP_RELATIVE interval 
      //====================================================================
      next = new_ui(ins + xed_decoded_inst_get_length(xptr),
		    RA_SP_RELATIVE, current->bp_ra_pos, current->bp_ra_pos,
		    current->bp_status, current->bp_bp_pos, current->bp_bp_pos,
		    current);
    } else if (is_reg_bp(reg0) && is_reg_sp(reg1)) {
      //====================================================================
      // instruction: initialize BP with value of SP to set up a frame ptr
      // action:      begin a new SP_RELATIVE interval 
      //====================================================================
      next = new_ui(ins + xed_decoded_inst_get_length(xptr), 
		    RA_STD_FRAME,
		    current->sp_ra_pos, current->sp_ra_pos, BP_SAVED,
		    current->sp_bp_pos, current->sp_bp_pos, current); 
      if (HW_TEST_STATE(highwatermark->state, HW_BP_SAVED, 
			HW_BP_OVERWRITTEN)) { 
	highwatermark->uwi = next;
	highwatermark->state = 
	  HW_NEW_STATE(highwatermark->state, HW_BP_OVERWRITTEN);
      }
    } else if (is_reg_bp(reg1) && is_reg_ax(reg0)) {
      //====================================================================
      // instruction: copy BP to RAX
      //====================================================================
      *rax_rbp_equivalent_at = ins + xed_decoded_inst_get_length(xptr);
    } else if (is_reg_bp(reg0)) {
      if (current->bp_status != BP_HOSED){
	//==================================================================
	// instruction: move some NON-special register to BP
	// state:       bp_status is NOT BP_HOSED
	// action:      begin a new RA_SP_RELATIVE,BP_HOSED interval
	//==================================================================
	next = new_ui(ins + xed_decoded_inst_get_length(xptr), 
		      RA_SP_RELATIVE,
		      current->sp_ra_pos, current->sp_ra_pos, BP_HOSED,
		      current->sp_bp_pos, current->sp_bp_pos, current);
	if (HW_TEST_STATE(highwatermark->state, HW_BP_SAVED, 
			  HW_BP_OVERWRITTEN) && 
	    (highwatermark->uwi->sp_ra_pos == next->sp_ra_pos)) {
	  highwatermark->uwi = next;
	  highwatermark->state = 
	    HW_NEW_STATE(highwatermark->state, HW_BP_OVERWRITTEN);
	}
      }
    }
  }
  return next;
}
