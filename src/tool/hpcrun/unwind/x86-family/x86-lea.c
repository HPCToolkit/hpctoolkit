#include "x86-interval-highwatermark.h"
#include "x86-decoder.h"
#include "x86-lea.h"
#include "x86-interval-arg.h"

unwind_interval *
process_lea(xed_decoded_inst_t *xptr, const xed_inst_t *xi, interval_arg_t *iarg)
{
  highwatermark_t *hw_tmp = &(iarg->highwatermark);
  unwind_interval *next = iarg->current;
  const xed_operand_t *op0 =  xed_inst_operand(xi, 0);
  xed_operand_enum_t   op0_name = xed_operand_name(op0);

  if ((op0_name == XED_OPERAND_REG0)) { 
    xed_reg_enum_t regname = xed_decoded_inst_get_reg(xptr, op0_name);
    if (is_reg_bp(regname)) {
      //=======================================================================
      // action: clobbering the base pointer; begin a new SP_RELATIVE interval 
      // note: we don't check that BP is BP_SAVED; we might have to
      //=======================================================================
      next = new_ui(iarg->ins + xed_decoded_inst_get_length(xptr),
		    RA_SP_RELATIVE, iarg->current->sp_ra_pos, iarg->current->bp_ra_pos,
		    BP_HOSED, iarg->current->sp_bp_pos, iarg->current->bp_bp_pos,
		    iarg->current);
      if (HW_TEST_STATE(hw_tmp->state, HW_BP_SAVED,
			HW_BP_OVERWRITTEN) &&
	  (hw_tmp->uwi->sp_ra_pos == next->sp_ra_pos)) {
	hw_tmp->uwi = next;
	hw_tmp->state = 
	  HW_NEW_STATE(hw_tmp->state, HW_BP_OVERWRITTEN);
      }
    }
  }
  return next;
}
