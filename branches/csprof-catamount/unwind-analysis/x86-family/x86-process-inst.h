#ifndef x86_process_inst_h
#define x86_process_inst_h

#include "x86-unwind-analysis.h"

unwind_interval *process_inst(xed_decoded_inst_t *xptr, char *ins, char *end, 
			      unwind_interval *current, unwind_interval *first,
			      bool *bp_just_pushed, highwatermark_t *highwatermark,
			      unwind_interval **canonical_interval, 
			      bool *bp_frames_found);

#endif

