#ifndef x86_process_inst_h
#define x86_process_inst_h

#include "x86-interval-highwatermark.h"
#include "x86-interval-arg.h"

unwind_interval *process_inst(xed_decoded_inst_t *xptr, void **ins_ptr, void *end,
			      unwind_interval **current_ptr, unwind_interval *first,
			      bool *bp_just_pushed, highwatermark_t *highwatermark,
			      unwind_interval **canonical_interval, 
			      bool *bp_frames_found, char **rax_rbp_equivalent_at,
                              interval_arg_t *iarg);

#endif
