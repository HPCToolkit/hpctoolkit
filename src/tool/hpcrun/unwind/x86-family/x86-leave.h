#ifndef x86_leave_h
#define x86_leave_h

#include "x86-unwind-analysis.h"
#include "x86-interval-arg.h"

/******************************************************************************
 * interface operations 
 *****************************************************************************/

unwind_interval *
process_leave(xed_decoded_inst_t *xptr, const xed_inst_t *xi, 
	      interval_arg_t *iarg);

#endif
