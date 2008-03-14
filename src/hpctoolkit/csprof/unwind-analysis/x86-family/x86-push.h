#ifndef x86_push_h
#define x86_push_h

/******************************************************************************
 * include files
 *****************************************************************************/

#include "x86-unwind-analysis.h"

/******************************************************************************
 * interface operations 
 *****************************************************************************/

unwind_interval *
process_push(char *ins, xed_decoded_inst_t *xptr, const xed_inst_t *xi, 
	     unwind_interval *current, highwatermark_t *highwatermark);



unwind_interval *
process_pop(char *ins, xed_decoded_inst_t *xptr, const xed_inst_t *xi, 
	    unwind_interval *current, highwatermark_t *highwatermark);

#endif
