#ifndef x86_push_h
#define x86_push_h

/******************************************************************************
 * include files
 *****************************************************************************/

#include "x86-interval-highwatermark.h"
#include "x86-interval-arg.h"

/******************************************************************************
 * interface operations 
 *****************************************************************************/

unwind_interval *
process_push(xed_decoded_inst_t *xptr, const xed_inst_t *xi, interval_arg_t *iarg);

unwind_interval *
process_pop(xed_decoded_inst_t *xptr, const xed_inst_t *xi, interval_arg_t *iarg);

#endif
