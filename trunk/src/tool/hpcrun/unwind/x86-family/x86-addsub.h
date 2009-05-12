#ifndef x86_addsub_h
#define x86_addsub_h

#include "x86-unwind-analysis.h"
#include "x86-interval-arg.h"

unwind_interval *
process_addsub(xed_decoded_inst_t *xptr, const xed_inst_t *xi, interval_arg_t *iarg);

#endif
