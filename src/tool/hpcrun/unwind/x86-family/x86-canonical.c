#include "x86-unwind-analysis.h"
#include "x86-decoder.h"
#include "x86-unwind-interval.h"
#include "pmsg.h"


#define HW_INITIALIZED		0x8
#define HW_BP_SAVED			0x4
#define HW_BP_OVERWRITTEN	0x2
#define HW_SP_DECREMENTED	0x1
#define HW_UNINITIALIZED	0x0


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
    if (bp_frames_found) { 
      // look for first bp frame
      first = find_first_bp_frame(first);
      set_ui_canonical(first, *canonical_interval);
      *canonical_interval = first;
    } else if (crhs) {
      if (hw_uwi && hw_uwi->bp_status != BP_UNCHANGED)
	if (crhs->bp_status == BP_UNCHANGED || 
		(crhs->bp_status == BP_SAVED && hw_uwi->bp_status == BP_HOSED)) {
         set_ui_canonical(hw_uwi, *canonical_interval);
         *canonical_interval = hw_uwi;
      }
      first = *canonical_interval;
    } else { 
      // look for first nondecreasing with no jmp
      first = find_first_non_decr(first, hw_uwi);
      set_ui_canonical(first, *canonical_interval);
      *canonical_interval = first;
    }
    {
      ra_loc ra_status = first->ra_status;
      bp_loc bp_status = (current->bp_status == BP_HOSED) ? BP_HOSED : first->bp_status;
#ifndef FIX_INTERVALS_AT_RETURN
      if ((current->ra_status != ra_status) ||
	  (current->bp_status != bp_status) ||
	  (current->sp_ra_pos != first->sp_ra_pos) ||
	  (current->bp_ra_pos != first->bp_ra_pos) ||
	  (current->bp_bp_pos != first->bp_bp_pos) ||
	  (current->sp_bp_pos != first->sp_bp_pos)) 
#endif
	{
	*next = new_ui(ins + xed_decoded_inst_get_length(xptr),
		       ra_status, first->sp_ra_pos, first->bp_ra_pos,
		       bp_status, first->sp_bp_pos, first->bp_bp_pos,
		       current);
        set_ui_restored_canonical(*next, (*canonical_interval)->prev_canonical);
        if (first->bp_status != BP_HOSED && bp_status == BP_HOSED) {
          set_ui_canonical(*next, *canonical_interval);
          *canonical_interval = *next;
        }
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
  while (first && (first->ra_status != RA_BP_FRAME)) first = (unwind_interval *)(first->common).next;
  return first;
}

unwind_interval *
find_first_non_decr(unwind_interval *first, unwind_interval *highwatermark)
{
  while (first && (first->common).next && (first->sp_ra_pos <= ((unwind_interval *)((first->common).next))->sp_ra_pos) && 
	 (first != highwatermark)) {
    first = (unwind_interval *)(first->common).next;
  }
  return first;
}

