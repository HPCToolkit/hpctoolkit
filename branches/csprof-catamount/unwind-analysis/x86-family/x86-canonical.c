#include "x86-unwind-analysis.h"
#include "x86-decoder.h"
#include "intervals.h"
#include "pmsg.h"

/******************************************************************************
 * forward declarations 
 *****************************************************************************/
unwind_interval *find_first_bp_frame(unwind_interval *first);

unwind_interval *find_first_non_decr(unwind_interval *first, 
				     unwind_interval *highwatermark);



/******************************************************************************
 * interface operations
 *****************************************************************************/

void 
reset_to_canonical_interval(xed_decoded_inst_t *xptr, unwind_interval *current, 
			    unwind_interval **next, char *ins, char *end, 
			    bool irdebug, unwind_interval *first, 
			    highwatermark_t *highwatermark, 
			    unwind_interval **canonical_interval, 
			    bool bp_frames_found)
{
  unwind_interval *hw_uwi = highwatermark->uwi;
  // if the return is not the last instruction in the interval, 
  // set up an interval for code after the return 
  if (ins + xed_decoded_inst_get_length(xptr) < end){
    unwind_interval *crhs = *canonical_interval;
    if (crhs) {
#if 0
      if ((hw_uwi && hw_uwi->bp_status == BP_SAVED) && 
	  (crhs->bp_status != BP_SAVED) &&
	  (crhs->sp_ra_pos == hw_uwi->sp_ra_pos)) *canonical_interval = hw_uwi;
      if ((hw_uwi && hw_uwi->bp_status == BP_SAVED) && 
	  (crhs->bp_status != BP_SAVED)) *canonical_interval = hw_uwi;
#endif
      if (hw_uwi && hw_uwi->bp_status == BP_SAVED)  
	if (crhs->bp_status != BP_SAVED) *canonical_interval = hw_uwi;
      first = *canonical_interval;
    } else if (bp_frames_found){ 
      // look for first bp frame
      first = find_first_bp_frame(first);
      *canonical_interval = first;
    } else { 
      // look for first nondecreasing with no jmp
      first = find_first_non_decr(first, hw_uwi);
      *canonical_interval = first;
    }
    {
#if 0
      ra_loc istatus =  
	(first->ra_status == RA_STD_FRAME) ? RA_BP_FRAME : first->ra_status;
#else
      ra_loc istatus = first->ra_status;
#endif
      if ((current->ra_status != istatus) ||
	  (current->bp_status != first->bp_status) ||
	  (current->sp_ra_pos != first->sp_ra_pos) ||
	  (current->bp_ra_pos != first->bp_ra_pos) ||
	  (current->bp_bp_pos != first->bp_bp_pos) ||
	  (current->sp_bp_pos != first->sp_bp_pos)) {
	*next = new_ui(ins + xed_decoded_inst_get_length(xptr),
		       istatus, first->sp_ra_pos, first->bp_ra_pos,
		       first->bp_status, first->sp_bp_pos, first->bp_bp_pos,
		       current);
	return;
      }
    }
  }
  *next = current; 
}



/******************************************************************************
 * private operations
 *****************************************************************************/


unwind_interval *
find_first_bp_frame(unwind_interval *first)
{
  while (first && (first->ra_status != RA_BP_FRAME)) first = first->next;
  return first;
}

unwind_interval *
find_first_non_decr(unwind_interval *first, unwind_interval *highwatermark)
{
  while (first && first->next && (first->sp_ra_pos <= first->next->sp_ra_pos) && 
	 (first != highwatermark)) {
    first = first->next;
  }
  return first;
}

