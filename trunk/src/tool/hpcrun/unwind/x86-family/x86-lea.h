#ifndef x86_lea_h
#define x86_lea_h

#include "x86-unwind-analysis.h"
#include "x86-interval-arg.h"

unwind_interval *
process_lea(xed_decoded_inst_t *xptr, const xed_inst_t *xi, interval_arg_t *iarg);

#endif

