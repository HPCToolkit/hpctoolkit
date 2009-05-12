/******************************************************************************
 * include files
 *****************************************************************************/

#include "x86-interval-highwatermark.h"
#include "x86-decoder.h"
#include "x86-interval-arg.h"

#include <assert.h>

/******************************************************************************
 * interface operations 
 *****************************************************************************/

unwind_interval *
process_push(xed_decoded_inst_t *xptr, const xed_inst_t *xi, interval_arg_t *iarg)
{
  unwind_interval *next;

  const xed_operand_t *op0 =  xed_inst_operand(xi, 0);
  xed_operand_enum_t   op0_name = xed_operand_name(op0);
  bp_loc bp_status = iarg->current->bp_status;
  int sp_bp_pos, size;

  switch(iclass(xptr)) {
  case XED_ICLASS_PUSH:   size = 8; break; // FIXME: assume 64-bit mode
  case XED_ICLASS_PUSHFQ: size = 8; break;
  case XED_ICLASS_PUSHFD: size = 4; break;
  case XED_ICLASS_PUSHF:  size = 2; break;
  default: assert(0);
  }

  sp_bp_pos = iarg->current->sp_bp_pos + size; 
  if (op0_name == XED_OPERAND_REG0) { 
    xed_reg_enum_t regname = xed_decoded_inst_get_reg(xptr, op0_name);
    if (is_reg_bp(regname) && bp_status == BP_UNCHANGED) {
      bp_status = BP_SAVED;
      sp_bp_pos = 0;
    }
  }

  next = new_ui(iarg->ins + xed_decoded_inst_get_length(xptr), iarg->current->ra_status, 
		iarg->current->sp_ra_pos + size, iarg->current->bp_ra_pos, bp_status,
		sp_bp_pos, iarg->current->bp_bp_pos, iarg->current);

  return next;
}


unwind_interval *
process_pop(xed_decoded_inst_t *xptr, const xed_inst_t *xi, interval_arg_t *iarg)
{
  unwind_interval *next;

  const xed_operand_t *op0 =  xed_inst_operand(xi, 0);
  xed_operand_enum_t   op0_name = xed_operand_name(op0);
  bp_loc bp_status = iarg->current->bp_status;
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
    if (is_reg_bp(regname)) bp_status = BP_UNCHANGED;
  }

  next = new_ui(iarg->ins + xed_decoded_inst_get_length(xptr), iarg->current->ra_status, 
		iarg->current->sp_ra_pos + size, iarg->current->bp_ra_pos, bp_status, 
		iarg->current->sp_bp_pos + size, iarg->current->bp_bp_pos, iarg->current);
  return next;
}
