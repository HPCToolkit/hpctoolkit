#ifndef x86_enter_h
#define x86_enter_h

/******************************************************************************
 * include files
 *****************************************************************************/

#include "xed-interface.h"
#include "x86-unwind-interval.h"
#include "x86-unwind-analysis.h"
#include "x86-interval-arg.h"


/******************************************************************************
 * interface operations 
 *****************************************************************************/

unwind_interval * process_enter(xed_decoded_inst_t *xptr, const xed_inst_t *xi, interval_arg_t *iarg);

#endif


