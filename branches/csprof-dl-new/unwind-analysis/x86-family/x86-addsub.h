#ifndef x86_addsub_h
#define x86_addsub_h

#include "x86-unwind-analysis.h"

unwind_interval *
process_addsub(char *ins, xed_decoded_inst_t *xptr, const xed_inst_t *xi, 
	       unwind_interval *current, highwatermark_t *highwatermark,
	       unwind_interval **canonical_interval,
	       bool *bp_frames_found);

#endif

