// -*-Mode: C++;-*- // technically C99
// $Id$

//************************* System Include Files ****************************

//*************************** User Include Files ****************************

#include <stdbool.h>

#include "pmsg.h"
#include "splay-interval.h"
#include "x86-unwind-interval.h"

#include "ui_tree.h"

#include "x86-decoder.h"
#include "x86-process-inst.h"
#include "x86-unwind-analysis.h"
#include "x86-unwind-interval-fixup.h"
#include "x86-interval-arg.h"

/******************************************************************************
 * forward declarations 
 *****************************************************************************/

static void set_status(interval_status *status, char *fui, int errcode, 
		       unwind_interval *first);

interval_status x86_build_intervals(void *ins, unsigned int len, int noisy);

static void x86_coalesce_unwind_intervals(unwind_interval *ui);

/******************************************************************************
 * interface operations 
 *****************************************************************************/

interval_status 
build_intervals(char *ins, unsigned int len)
{
  return x86_build_intervals(ins, len, 0);
}


interval_status 
x86_build_intervals(void *ins, unsigned int len, int noisy)
{

  interval_status status;

  xed_decoded_inst_t xedd;
  xed_decoded_inst_t *xptr = &xedd;
  xed_error_enum_t xed_error;

  // the local data structure for handling all of the various data needed for
  // building intervals.
  //

  interval_arg_t iarg;

  highwatermark_t _h = { NULL, NULL, HW_UNINITIALIZED };

  int error_count = 0;

  unwind_interval *next = NULL;

  // local allocation of iarg data

  void *first_ins    = ins;
  void *end          = ins + len;

  iarg.beg           = first_ins;
  iarg.end           = end;
  iarg.highwatermark = _h;
  iarg.ins           = ins;
  iarg.current       = new_ui(ins, RA_SP_RELATIVE, 0, 0, BP_UNCHANGED, 0, 0, NULL);
  iarg.first         = iarg.current;

  // handle return is different if there are any bp frames

  iarg.bp_frames_found 	     = false;
  iarg.bp_just_pushed  	     = false;

  iarg.rax_rbp_equivalent_at = NULL;
  iarg.canonical_interval    = NULL;

  xed_decoded_inst_zero_set_mode(xptr, &x86_decoder_settings.xed_settings);

  if (noisy) dump_ui(iarg.current, true);

  while (iarg.ins < end) {
    xed_decoded_inst_zero_keep_mode(xptr);
    xed_error = xed_decode(xptr, (uint8_t*) iarg.ins, 15);

    if (xed_error != XED_ERROR_NONE) {
      error_count++; /* note the error      */
      iarg.ins++;         /* skip this byte      */
      continue;      /* continue onward ... */
    }

    // ensure that we don't move past the end of the interval because of a misaligned instruction
    void *nextins = iarg.ins + xed_decoded_inst_get_length(xptr);
    if (nextins > end) break;

    next = process_inst(xptr, &iarg.ins, iarg.end, &iarg.current, iarg.first, &iarg.bp_just_pushed,
			&iarg.highwatermark, &iarg.canonical_interval, &iarg.bp_frames_found,
			&iarg.rax_rbp_equivalent_at,
			&iarg);
    
    if (next == &poison_ui) {
      set_status(&status, iarg.ins, -1, NULL);
      return status;
    }

    if (next != iarg.current) {
      link_ui(iarg.current, next);
      iarg.current = next;

      if (noisy) dump_ui(iarg.current, true);
    }
    iarg.ins += xed_decoded_inst_get_length(xptr);
    (iarg.current->common).end = iarg.ins;
  }

  (iarg.current->common).end = end;

  set_status(&status, iarg.ins, error_count, iarg.first);

  x86_fix_unwind_intervals(iarg.beg, len, &status);
  x86_coalesce_unwind_intervals((unwind_interval *)status.first);
  return status;
}

bool
x86_ui_same_data(unwind_interval *proto, unwind_interval *cand)
{
  return ( (proto->ra_status == cand->ra_status) &&
	   (proto->sp_ra_pos == cand->sp_ra_pos) &&
	   (proto->sp_bp_pos == cand->sp_bp_pos) &&
	   (proto->bp_status == cand->bp_status) &&
	   (proto->bp_ra_pos == cand->bp_ra_pos) &&
	   (proto->bp_bp_pos == cand->bp_bp_pos) );
}


/******************************************************************************
 * private operations 
 *****************************************************************************/

static void
set_status(interval_status *status, char *fui, int errcode, 
	   unwind_interval *first)
{
  status->first_undecoded_ins = fui;
  status->errcode = errcode;
  status->first   = (splay_interval_t *)first;
}

/* static */ void
x86_coalesce_unwind_intervals(unwind_interval *ui)
{
  TMSG(COALESCE,"coalescing interval list starting @ %p",ui);
  if (! ui) {
    TMSG(COALESCE,"  --interval list empty, so no work");
    return;
  }

  unwind_interval *current = ui;
  ui                       = (unwind_interval *) ui->common.next;
  
  TMSG(COALESCE," starting prototype interval =");
  if (ENABLED(COALESCE)){
    dump_ui_log(current);
  }
  for (; ui; ui = (unwind_interval *)ui->common.next) {
    TMSG(COALESCE,"comparing this interval to prototype:");
    if (ENABLED(COALESCE)){
      dump_ui_log(ui);
    }
    
    if (x86_ui_same_data(current,ui)){
      current->common.end  = ui->common.end;
      current->common.next = ui->common.next;
      free_ui_node_locked((interval_tree_node *) ui);
      ui = current;
      TMSG(COALESCE,"Intervals match! Extended interval:");
      if (ENABLED(COALESCE)){
	dump_ui_log(current);
      }
    }
    else {
      TMSG(COALESCE,"Interval does not match prototype. Reset prototype to current interval");
      current = ui;
    }
  }
  return;
}
