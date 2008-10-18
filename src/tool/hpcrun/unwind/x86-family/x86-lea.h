#ifndef x86_lea_h
#define x86_lea_h

#include "x86-unwind-analysis.h"

unwind_interval *
process_lea(char *ins, xed_decoded_inst_t *xptr, const xed_inst_t *xi, 
	    unwind_interval *current, highwatermark_t *highwatermark);

#endif

