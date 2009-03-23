/******************************************************************************
 * includes
 *****************************************************************************/

#include "pmsg.h"

#include "x86-build-intervals.h"
#include "x86-decoder.h"
#include "x86-process-inst.h"
#include "x86-canonical.h"
#include "x86-jump.h"
#include "x86-interval-highwatermark.h"


/******************************************************************************
 * interface operations
 *****************************************************************************/

unwind_interval *
process_unconditional_branch(xed_decoded_inst_t *xptr, unwind_interval *current,
			    char *ins, char *end, 
			    bool irdebug, unwind_interval *first, 
			    highwatermark_t *highwatermark, 
			    unwind_interval **canonical_interval, 
			    bool bp_frames_found)
{
  unwind_interval *next = current;

  if (highwatermark->state == HW_UNINITIALIZED) {
    highwatermark->uwi = current;
    highwatermark->state = HW_INITIALIZED;
  }

  reset_to_canonical_interval(xptr, current, &next, ins, end, irdebug, first, 
			      highwatermark, canonical_interval, 
			      bp_frames_found); 
  return next;
}


unwind_interval *
process_conditional_branch(unwind_interval *current, 
			   highwatermark_t *highwatermark)
{
  if (highwatermark->state == HW_UNINITIALIZED) {
    highwatermark->uwi = current;
    highwatermark->state = HW_INITIALIZED;
  }

  return current;
}
