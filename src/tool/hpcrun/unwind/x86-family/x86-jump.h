#ifndef x86_jump_h
#define x86_jump_h

/******************************************************************************
 * includes
 *****************************************************************************/

#include "x86-unwind-analysis.h"

/******************************************************************************
 * interface operations
 *****************************************************************************/

unwind_interval *
process_unconditional_branch(xed_decoded_inst_t *xptr, bool irdebug, interval_arg_t *iarg);

unwind_interval *
process_conditional_branch(xed_decoded_inst_t *xptr,
                           unwind_interval *current,
                           void *ins, void *end,
                           unwind_interval *first,
			   highwatermark_t *highwatermark);

#endif
