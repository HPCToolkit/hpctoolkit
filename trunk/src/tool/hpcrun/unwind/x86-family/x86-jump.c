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
#include "x86-unwind-analysis.h"


/******************************************************************************
 * interface operations
 *****************************************************************************/

unwind_interval *
process_unconditional_branch(xed_decoded_inst_t *xptr, unwind_interval *current,
			    char *ins, void *end,
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

  TMSG(TAIL_CALL,"checking for tail call via unconditional branch @ %p",ins);
  void *possible = x86_get_branch_target(ins, xptr);
  if (possible == NULL) {
    TMSG(TAIL_CALL,"indirect unconditional branch ==> possible tail call");
    next->has_tail_calls = true;
  }
  else if ((possible > end) || (possible < first->common.start)) {
    TMSG(TAIL_CALL,"unconditional branch to address %p outside of current routine (%p to %p)",
         possible, first->common.start, end);
    next->has_tail_calls = true;
  }

  return next;
}


unwind_interval *
process_conditional_branch(xed_decoded_inst_t *xptr,
                           unwind_interval *current,
                           void *ins, void *end,
                           unwind_interval *first,
			   highwatermark_t *highwatermark)
{
  if (highwatermark->state == HW_UNINITIALIZED) {
    highwatermark->uwi = current;
    highwatermark->state = HW_INITIALIZED;
  }

  TMSG(TAIL_CALL,"checking for tail call via unconditional branch @ %p",ins);
  void *possible = x86_get_branch_target(ins, xptr);
  if (possible == NULL) {
    TMSG(TAIL_CALL,"indirect unconditional branch ==> possible tail call");
    current->has_tail_calls = true;
  }
  else if ((possible > end) || (possible < first->common.start)) {
    TMSG(TAIL_CALL,"unconditional branch to address %p outside of current routine (%p to %p)",
         possible, first->common.start, end);
    current->has_tail_calls = true;
  }

  return current;
}
