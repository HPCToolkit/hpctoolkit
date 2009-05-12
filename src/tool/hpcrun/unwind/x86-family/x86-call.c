#include "x86-interval-highwatermark.h"
#include "x86-decoder.h"
#include "x86-interval-arg.h"

/******************************************************************************
 * interface operations
 *****************************************************************************/

unwind_interval*
process_call(interval_arg_t *iarg)
{
  unwind_interval *next = iarg->current;
  highwatermark_t *hw_tmp = &(iarg->highwatermark);

  if (hw_tmp->state == HW_UNINITIALIZED) {
    hw_tmp->uwi = iarg->current;
    hw_tmp->state = HW_INITIALIZED;
  }
  
#ifdef USE_CALL_LOOKAHEAD
  next = call_lookahead(xptr, iarg->current, iarg->ins);
#endif
  return next;
}



/******************************************************************************
 * private operations
 *****************************************************************************/

#undef USE_CALL_LOOKAHEAD
#ifdef USE_CALL_LOOKAHEAD
unwind_interval *
call_lookahead(xed_decoded_inst_t *call_xedd, unwind_interval *current, char *ins)
{
  // Assumes: 'ins' is pointing at the instruction from which
  // lookahead is to occur (i.e, the instruction prior to the first
  // lookahead).

  unwind_interval *next;
  xed_error_enum_t xed_err;
  int length = call_xedd->get_length();
  xed_decoded_inst_t xeddobj;
  xed_decoded_inst_t* xedd = &xeddobj;
  char *jmp_ins_addr = ins + length;
  char *jmp_target = NULL;
  char *jmp_succ_addr = NULL;

  if (current->ra_status == RA_BP_FRAME) {
    return current;
  }

  // -------------------------------------------------------
  // requirement 1: unconditional jump with known target within routine
  // -------------------------------------------------------
  xed_decoded_inst_zero_set_mode(xptr, xed_machine_state_ptr);
  xed_err = xed_decode(xptr, reinterpret_cast<const uint8_t*>(jmp_ins_addr), 15);
  if (xed_err != XED_ERROR_NONE) {
    return current;
  }

  if (iclass_eq(xptr, XED_ICLASS_JMP) || 
      iclass_eq(xptr, XED_ICLASS_JMP_FAR)) {
    if (xed_decoded_inst_number_of_memory_operands(xptr) == 0) {
      const xed_immdis_t& disp = xptr->get_disp();
      if (disp.is_present()) {
	long long offset = disp.get_signed64();
	jmp_succ_addr = jmp_ins_addr + xptr->get_length();
	jmp_target = jmp_succ_addr + offset;
      }
    }
  }
  if (jmp_target == NULL) {
    // jump of proper type not recognized 
    return current;
  }
  // FIXME: possibly test to ensure jmp_target is within routine

  // -------------------------------------------------------
  // requirement 2: jump target affects stack
  // -------------------------------------------------------
  xed_decoded_inst_zero_set_mode(xptr, xed_machine_state_ptr);
  xed_err = xed_decode(xptr, reinterpret_cast<const uint8_t*>(jmp_target), 15);
  if (xed_err != XED_ERROR_NONE) {
    return current;
  }
  
  if (iclass_eq(xptr, XED_ICLASS_SUB) || iclass_eq(xptr, XED_ICLASS_ADD)) {
    const xed_operand_t* op0 = xed_inst_operand(xi,0);
    if ((xed_operand_name(op0) == XED_OPERAND_REG) && (xed_operand_reg(op0) == XED_REG_RSP)) {
      const xed_immdis_t& immed = xptr->get_immed();
      if (immed.is_present()) {
	int sign = (iclass_eq(xptr, XED_ICLASS_ADD)) ? -1 : 1;
	long offset = sign * immed.get_signed64();
	PMSG(INTV,"newinterval from ADD/SUB immediate");
	next = newinterval(jmp_succ_addr,
			   current->ra_status,
			   current->sp_ra_pos + offset,
			   current->bp_ra_pos,
			   current->bp_status,
			   current->sp_bp_pos + offset,
			   current->bp_bp_pos,
			   current);
        return next;
      }
    }
  }
  return current;
}
#endif

