/******************************************************************************
 * includes
 *****************************************************************************/

#include "pmsg.h"

#include "x86-build-intervals.h"
#include "x86-decoder.h"
#include "x86-process-inst.h"
#include "x86-canonical.h"
#include "x86-jump.h"

/******************************************************************************
 * interface operations
 *****************************************************************************/

unwind_interval *
process_unconditional_branch(xed_decoded_inst_t *xptr, unwind_interval *current,
			    char *ins, char *end, 
			    bool irdebug, unwind_interval *first, 
			    highwatermark_t *highwatermark, 
			    unwind_interval **canonical_interval, 
			    bool bp_frames_found)
{
  unwind_interval *next = current;
  if (highwatermark->type == HW_NONE)  {
    highwatermark->uwi = current; 
    highwatermark->type = HW_BRANCH; 
  }

  reset_to_canonical_interval(xptr, current, &next, ins, end, irdebug, first, 
			      highwatermark, canonical_interval, bp_frames_found); 
  return next;
}


unwind_interval *
process_conditional_branch(unwind_interval *current, 
			   highwatermark_t *highwatermark)
{
  if (highwatermark->type == HW_NONE)  {
    highwatermark->uwi = current; 
    highwatermark->type = HW_BRANCH; 
  } 
  return current;
}



/******************************************************************************
 * dead code
 *****************************************************************************/

#if 0
jump_lookahead() 
{
  if (xed_decoded_inst_number_of_memory_operands(xptr) == 0) {
    const xed_immdis_t& disp =  xed_operand_values_get_displacement_for_memop(xptr);
    xed_operand_values_t *xopv;
    if (xed_operand_values_has_memory_displacement(xopv)) {
      long long offset = 
	xed_operand_values_get_memory_displacement_int64(xopv);
      char *target = ins + offset;
      if (jdebug) {
	xedd.dump(cout);
	cerr << "JMP offset = " << offset << ", target = " << (void *) target << ", start = " << (void *) start << ", end = " << (void *) end << endl;
      } 
      if (target < start || target > end) {
	reset_to_canonical_interval(xptr, current, next, ins, end, irdebug, 
				    first, &highwatermark, canonical_interval, bp_frames_found); 
      }
    }
    if (xedd.get_operand_count() >= 1) { 
      const xed_inst_t *xi = xed_decoded_inst_inst(xptr);
      const xed_operand_t *op0 =  xed_inst_operand(xi,0);
      if ((xed_operand_name(op0) == XED_OPERAND_REG) && 
	  (current->ra_status == RA_SP_RELATIVE) && 
	  (current->sp_ra_pos == 0)) {
	// jump to a register when the stack is in an SP relative state
	// with the return address at offset 0 in the stack.
	// a good assumption is that this is a tail call. -- johnmc
	// (change 1)
	reset_to_canonical_interval(xptr, current, next, ins, end, irdebug, 
				    first, &highwatermark, canonical_interval, bp_frames_found); 
      }
    }
  }
}
#endif

