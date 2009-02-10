#include "x86-unwind-analysis.h"
#include "x86-lea.h"

unwind_interval *
process_lea(char *ins, xed_decoded_inst_t *xptr, const xed_inst_t *xi, 
	     unwind_interval *current, highwatermark_t *highwatermark)
{
  unwind_interval *next = current;
    const xed_operand_t *op0 =  xed_inst_operand(xi, 0);
    // const xed_operand_t *op1 =  xed_inst_operand(xi, 1); // unused

    xed_operand_enum_t   op0_name = xed_operand_name(op0);
    //xed_operand_enum_t   op1_name = xed_operand_name(op1); // unused

    if ((op0_name == XED_OPERAND_REG0)) { 
      if (xed_decoded_inst_get_reg(xptr, op0_name) == XED_REG_RBP) {
	//=========================================================================
	// action: clobbering the base pointer; begin a new SP_RELATIVE interval 
	// note: we don't check that BP is BP_SAVED; we might have to
	//=========================================================================
	next = new_ui(ins + xed_decoded_inst_get_length(xptr),
		      RA_SP_RELATIVE, current->sp_ra_pos, current->bp_ra_pos,
                      BP_HOSED, current->sp_bp_pos, current->bp_bp_pos,
		      current);
	if ((highwatermark->type == HW_BPSAVE_AFTER_SUB || highwatermark->type  == HW_BPSAVE) && (highwatermark->uwi->sp_ra_pos == next->sp_ra_pos)) {
          highwatermark->uwi = next;
          highwatermark->type = HW_BPHOSED;
        }

    }
  }
  return next;
}
