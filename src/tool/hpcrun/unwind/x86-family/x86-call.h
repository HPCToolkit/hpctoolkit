#ifndef x86_call_h
#define x86_call_h

#include "x86-unwind-interval.h"
#include "x86-unwind-analysis.h"

/******************************************************************************
 * interface operations
 *****************************************************************************/

unwind_interval*
process_call(unwind_interval *current, highwatermark_t *highwatermark);

#endif




