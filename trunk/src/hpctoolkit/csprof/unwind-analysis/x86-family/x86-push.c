/******************************************************************************
 * include files
 *****************************************************************************/

#include "x86-unwind-analysis.h"

/******************************************************************************
 * interface operations 
 *****************************************************************************/

unwind_interval *
process_push(char *ins, xed_decoded_inst_t *xptr, const xed_inst_t *xi, 
	     unwind_interval *current, highwatermark_t *highwatermark)
{
  unwind_interval *next;

  const xed_operand_t *op0 =  xed_inst_operand(xi, 0);
  xed_operand_enum_t   op0_name = xed_operand_name(op0);
  bp_loc bp_status = current->bp_status;
  int sp_bp_pos, size;

  switch(iclass(xptr)) {
  case XED_ICLASS_PUSH:   size = 8; break; // FIXME: assume 64-bit mode
  case XED_ICLASS_PUSHFQ: size = 8; break;
  case XED_ICLASS_PUSHFD: size = 4; break;
  case XED_ICLASS_PUSHF:  size = 2; break;
  default: assert(0);
  }

  sp_bp_pos = current->sp_bp_pos + size; 
  if (op0_name == XED_OPERAND_REG0) { 
    xed_reg_enum_t regname = xed_decoded_inst_get_reg(xptr, op0_name);
    if (regname == XED_REG_RBP && bp_status == BP_UNCHANGED) {
      bp_status = BP_SAVED;
      sp_bp_pos = 0;
    }
  }

  next = new_ui(ins + xed_decoded_inst_get_length(xptr), current->ra_status, 
		current->sp_ra_pos + size, current->bp_ra_pos, bp_status,
		sp_bp_pos, current->bp_bp_pos, current);

  return next;
}


unwind_interval *
process_pop(char *ins, xed_decoded_inst_t *xptr, const xed_inst_t *xi, 
	    unwind_interval *current, highwatermark_t *highwatermark)
{
  unwind_interval *next;

  const xed_operand_t *op0 =  xed_inst_operand(xi, 0);
  xed_operand_enum_t   op0_name = xed_operand_name(op0);
  bp_loc bp_status = current->bp_status;
  int size;

  switch(iclass(xptr)) {
  case XED_ICLASS_POP:   size = -8;  break; // FIXME: assume 64-bit mode
  case XED_ICLASS_POPFQ: size = -8;  break;
  case XED_ICLASS_POPFD: size = -4;  break;
  case XED_ICLASS_POPF:  size = -2;  break;
  default: assert(0);
  }

  if (op0_name == XED_OPERAND_REG0) { 
    xed_reg_enum_t regname = xed_decoded_inst_get_reg(xptr, op0_name);
    if (regname == XED_REG_RBP)  bp_status = BP_UNCHANGED;
  }

  next = new_ui(ins + xed_decoded_inst_get_length(xptr), current->ra_status, 
		current->sp_ra_pos + size, current->bp_ra_pos, bp_status, 
		current->sp_bp_pos + size, current->bp_bp_pos, current);
  return next;
}
