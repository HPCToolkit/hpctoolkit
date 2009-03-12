#ifndef x86_return_h
#define x86_return_h

/******************************************************************************
 * include files
 *****************************************************************************/
#include "x86-unwind-analysis.h"


/******************************************************************************
 * interface operations
 *****************************************************************************/

unwind_interval *
process_return(xed_decoded_inst_t *xptr, unwind_interval **current_ptr, 
	       void **ins, void *end, bool irdebug, unwind_interval *first, 
	       highwatermark_t *highwatermark, 
	       unwind_interval **canonical_interval, bool *bp_frames_found);

#endif


