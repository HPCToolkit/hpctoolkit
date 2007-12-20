#include "x86-unwind-analysis.h"

unwind_interval *
process_move(char *ins, xed_decoded_inst_t *xptr, const xed_inst_t *xi, 
	     unwind_interval *current, highwatermark_t *highwatermark)
{
  unwind_interval *next = current;
  if (xed_decoded_inst_get_base_reg(xptr, 0) == XED_REG_RSP) {
    //-------------------------------------------------------------------------
    // a memory move with SP as a base register
    //-------------------------------------------------------------------------
    const xed_operand_t *op0 =  xed_inst_operand(xi, 0);
    const xed_operand_t *op1 =  xed_inst_operand(xi, 1);

    xed_operand_enum_t   op0_name = xed_operand_name(op0);
    xed_operand_enum_t   op1_name = xed_operand_name(op1);
    
    if ((op0_name == XED_OPERAND_MEM0) && (op1_name == XED_OPERAND_REG0)) { 
      //-------------------------------------------------------------------------
      // storing a register to memory 
      //-------------------------------------------------------------------------
      if (xed_decoded_inst_get_reg(xptr, op1_name) == XED_REG_RBP)  {
	//-------------------------------------------------------------------------
	// register being stored is BP
	//-------------------------------------------------------------------------
	if (current->bp_status != BP_SAVED){
	  //=======================================================================
	  // instruction: save caller's BP into the stack  
	  // action:      create a new interval with 
	  //                (1) BP status reset to BP_SAVED
	  //                (2) BP position relative to the stack pointer set to 
	  //                    the offset from SP 
	  //=======================================================================
	  next = new_ui(ins + xed_decoded_inst_get_length(xptr),
			current->ra_status,current->sp_ra_pos,current->bp_ra_pos,
			BP_SAVED,
			xed_decoded_inst_get_memory_displacement(xptr, 0),
			current->bp_bp_pos,
			current);
	  highwatermark->uwi = next;
	  highwatermark->type = HW_BPSAVE;
	}
      }
    } else if ((op1_name == XED_OPERAND_MEM0) && (op0_name == XED_OPERAND_REG0)) { 
      //-------------------------------------------------------------------------
      // loading a register from memory 
      //-------------------------------------------------------------------------
      if (xed_decoded_inst_get_reg(xptr, op0_name) == XED_REG_RBP) {
	//-------------------------------------------------------------------------
	// register being loaded is BP
	//-------------------------------------------------------------------------
	if (current->bp_status == BP_SAVED) {
	  int64_t offset = xed_decoded_inst_get_memory_displacement(xptr, 0);
	  if (offset == current->sp_bp_pos) { 
	    //=====================================================================
	    // instruction: restore BP from its saved location in the stack  
	    // action:      create a new interval with BP status reset to 
	    //              BP_UNCHANGED
	    //=====================================================================
	    next = new_ui(ins + xed_decoded_inst_get_length(xptr),
			  current->ra_status, current->sp_ra_pos, 
			  current->bp_ra_pos, BP_UNCHANGED, current->sp_bp_pos, 
			  current->bp_bp_pos, current);
	  }
	}
      }
    }
  } else { 
    const xed_operand_t *op0 =  xed_inst_operand(xi, 0);
    const xed_operand_t *op1 =  xed_inst_operand(xi, 1);

    xed_operand_enum_t   op0_name = xed_operand_name(op0);
    xed_operand_enum_t   op1_name = xed_operand_name(op1);

    if ((op0_name == XED_OPERAND_REG0) && (op1_name == XED_OPERAND_REG1)) { 
      //-------------------------------------------------------------------------
      // register-to-register move 
      //-------------------------------------------------------------------------
      if ((xed_decoded_inst_get_reg(xptr, op1_name) == XED_REG_RBP) &&
	  (xed_decoded_inst_get_reg(xptr, op0_name) == XED_REG_RSP)) {
	//=========================================================================
	// instruction: restore SP from BP
	// action:      begin a new SP_RELATIVE interval 
	//=========================================================================
	next = new_ui(ins + xed_decoded_inst_get_length(xptr),
		      RA_SP_RELATIVE, current->bp_ra_pos, current->bp_ra_pos,
		      current->bp_status,current->bp_bp_pos, current->bp_bp_pos,
		      current);
      } else if ((xed_decoded_inst_get_reg(xptr, op0_name) == XED_REG_RBP) &&
		 (xed_decoded_inst_get_reg(xptr, op1_name) == XED_REG_RSP)) {
	//=========================================================================
	// instruction: initialize BP with value of SP to set up a frame pointer
	// action:      begin a new SP_RELATIVE interval 
	//=========================================================================
	next = new_ui(ins + xed_decoded_inst_get_length(xptr), 
#if PREFER_BP_FRAME
		      RA_BP_FRAME,
#else
		      RA_STD_FRAME,
#endif
		      current->sp_ra_pos, current->sp_ra_pos, BP_SAVED,
		      current->sp_bp_pos, current->sp_bp_pos, current); 
	highwatermark->uwi = next;
	highwatermark->type = HW_CREATE_STD;
      }
    }
  }
  return next;
}
