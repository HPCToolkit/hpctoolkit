#ifndef x86_enter_h
#define x86_enter_h

/******************************************************************************
 * include files
 *****************************************************************************/

#include "xed-interface.h"
#include "intervals.h"
#include "x86-unwind-analysis.h"


/******************************************************************************
 * interface operations 
 *****************************************************************************/

unwind_interval *
process_enter(char *ins, xed_decoded_inst_t *xptr, const xed_inst_t *xi, 
	      unwind_interval *current, highwatermark_t *highwatermark);

#endif


