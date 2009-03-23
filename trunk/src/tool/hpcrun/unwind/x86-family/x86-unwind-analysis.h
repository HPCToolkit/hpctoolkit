#ifndef x86_unwind_analysis_h
#define x86_unwind_analysis_h

#include "xed-interface.h"
#include "x86-unwind-interval.h"
#include "csprof-malloc.h"

extern void *x86_get_branch_target(void *ins,xed_decoded_inst_t *xptr);

#define FIX_INTERVALS_AT_RETURN
#endif  // x86_unwind_analysis_h
