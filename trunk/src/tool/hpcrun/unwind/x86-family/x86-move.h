#ifndef x86_move_h
#define x86_move_h

#include "x86-unwind-analysis.h"
#include "x86-interval-arg.h"

unwind_interval *
process_move(xed_decoded_inst_t *xptr, const xed_inst_t *xi, interval_arg_t *iarg);

#endif

