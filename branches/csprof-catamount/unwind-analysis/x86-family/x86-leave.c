#include "x86-unwind-analysis.h"

/******************************************************************************
 * interface operations 
 *****************************************************************************/

unwind_interval *
process_leave(char *ins, xed_decoded_inst_t *xptr, const xed_inst_t *xi, 
	      unwind_interval *current, highwatermark_t *highwatermark)
{
  unwind_interval *next;
  next = new_ui(ins + xed_decoded_inst_get_length(xptr), 
		RA_SP_RELATIVE, 0, 0, BP_UNCHANGED, 0, 0, current);
  return next;
}

