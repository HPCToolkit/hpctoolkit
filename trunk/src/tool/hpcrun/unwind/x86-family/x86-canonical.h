#ifndef x86_canonical_h
#define x86_canonical_h

#include "x86-unwind-analysis.h"


/******************************************************************************
 * interface operations
 *****************************************************************************/

void 
reset_to_canonical_interval(xed_decoded_inst_t *xptr, unwind_interval *current, 
			    unwind_interval **next, char *ins, char *end, 
			    bool irdebug, unwind_interval *first, 
			    highwatermark_t *highwatermark, 
			    unwind_interval **canonical_interval, 
			    bool bp_frames_found);

#endif
