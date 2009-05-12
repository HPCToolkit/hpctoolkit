#ifndef x86_canonical_h
#define x86_canonical_h

#include "x86-unwind-analysis.h"
#include "x86-interval-arg.h"


/******************************************************************************
 * interface operations
 *****************************************************************************/

void 
#ifdef IARGR
reset_to_canonical_interval(xed_decoded_inst_t *xptr,
			    unwind_interval **next,
			    bool irdebug,
                            interval_arg_t *iarg);
#endif
reset_to_canonical_interval(xed_decoded_inst_t *xptr, unwind_interval *current, 
			    unwind_interval **next, char *ins, char *end, 
			    bool irdebug, unwind_interval *first, 
			    highwatermark_t *highwatermark, 
			    unwind_interval **canonical_interval, 
			    bool bp_frames_found);

#endif
