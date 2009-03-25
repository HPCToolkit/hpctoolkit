#ifndef x86_move_h
#define x86_move_h

#include "x86-unwind-analysis.h"

unwind_interval *
process_move(char *ins, xed_decoded_inst_t *xptr, const xed_inst_t *xi, 
	     unwind_interval *current, highwatermark_t *highwatermark, void **rax_rbp_equivalent_at);

#endif

