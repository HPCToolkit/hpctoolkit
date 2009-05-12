#ifndef x86_return_h
#define x86_return_h

/******************************************************************************
 * include files
 *****************************************************************************/
#include "x86-unwind-analysis.h"
#include "x86-interval-arg.h"


/******************************************************************************
 * interface operations
 *****************************************************************************/

unwind_interval *
process_return(xed_decoded_inst_t *xptr, bool irdebug, interval_arg_t *iarg);

#endif


