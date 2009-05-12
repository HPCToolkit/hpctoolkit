#include "x86-interval-highwatermark.h"
#include "x86-interval-arg.h"

/******************************************************************************
 * interface operations 
 *****************************************************************************/

unwind_interval *
process_leave(xed_decoded_inst_t *xptr, const xed_inst_t *xi,
              interval_arg_t *iarg)
{
  unwind_interval *next;
  next = new_ui(iarg->ins + xed_decoded_inst_get_length(xptr), 
		RA_SP_RELATIVE, 0, 0, BP_UNCHANGED, 0, 0, iarg->current);
  return next;
}

