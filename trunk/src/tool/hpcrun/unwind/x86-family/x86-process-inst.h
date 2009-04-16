#ifndef x86_process_inst_h
#define x86_process_inst_h

#include "x86-interval-highwatermark.h"
#include "x86-interval-arg.h"

unwind_interval *process_inst(xed_decoded_inst_t *xptr, interval_arg_t *iarg);

#endif
