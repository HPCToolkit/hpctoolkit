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
#include "x86-interval-arg.h"


/******************************************************************************
 * interface operations
 *****************************************************************************/

unwind_interval *
process_unconditional_branch(xed_decoded_inst_t *xptr, bool irdebug, interval_arg_t *iarg)
{
  unwind_interval *next = iarg->current;

  if ((iarg->highwatermark).state == HW_UNINITIALIZED) {
    (iarg->highwatermark).uwi = iarg->current;
    (iarg->highwatermark).state = HW_INITIALIZED;
  }

#ifdef IARGR
  reset_to_canonical_interval(xptr, &next, irdebug, iarg);
#endif
  reset_to_canonical_interval(xptr, iarg->current, &next, iarg->ins, iarg->end, irdebug, iarg->first, 
			      &(iarg->highwatermark), &(iarg->canonical_interval),
			      iarg->bp_frames_found);

  TMSG(TAIL_CALL,"checking for tail call via unconditional branch @ %p",iarg->ins);
  void *possible = x86_get_branch_target(iarg->ins, xptr);
  if (possible == NULL) {
    TMSG(TAIL_CALL,"indirect unconditional branch ==> possible tail call");
    next->has_tail_calls = true;
  }
  else if ((possible > iarg->end) || (possible < iarg->first->common.start)) {
    TMSG(TAIL_CALL,"unconditional branch to address %p outside of current routine (%p to %p)",
         possible, iarg->first->common.start, iarg->end);
    next->has_tail_calls = true;
  }

  return next;
}


unwind_interval *
process_conditional_branch(xed_decoded_inst_t *xptr,
                           interval_arg_t *iarg)
{
  highwatermark_t *hw_tmp = &(iarg->highwatermark);
  if (hw_tmp->state == HW_UNINITIALIZED) {
    hw_tmp->uwi = iarg->current;
    hw_tmp->state = HW_INITIALIZED;
  }

  TMSG(TAIL_CALL,"checking for tail call via unconditional branch @ %p",iarg->ins);
  void *possible = x86_get_branch_target(iarg->ins, xptr);
  if (possible == NULL) {
    TMSG(TAIL_CALL,"indirect unconditional branch ==> possible tail call");
    iarg->current->has_tail_calls = true;
  }
  else if ((possible > iarg->end) || (possible < iarg->first->common.start)) {
    TMSG(TAIL_CALL,"unconditional branch to address %p outside of current routine (%p to %p)",
         possible, iarg->first->common.start, iarg->end);
    iarg->current->has_tail_calls = true;
  }

  return iarg->current;
}
