#ifndef x86_call_h
#define x86_call_h

#include "x86-unwind-interval.h"
#include "x86-unwind-analysis.h"
#include "x86-interval-arg.h"

/******************************************************************************
 * interface operations
 *****************************************************************************/

unwind_interval*
process_call(interval_arg_t *iarg);

#endif




