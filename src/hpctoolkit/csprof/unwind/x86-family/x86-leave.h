#ifndef x86_leave_h
#define x86_leave_h

#include "x86-unwind-analysis.h"

/******************************************************************************
 * interface operations 
 *****************************************************************************/

unwind_interval *
process_leave(char *ins, xed_decoded_inst_t *xptr, const xed_inst_t *xi, 
	      unwind_interval *current, highwatermark_t *highwatermark);

#endif

