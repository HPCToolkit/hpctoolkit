// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2016, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

//************************* System Include Files ****************************

//*************************** User Include Files ****************************

#include <stdbool.h>

#include <include/hpctoolkit-config.h>
#include "splay-interval.h"
#include "x86-unwind-interval.h"

#include "ui_tree.h"

#include "x86-decoder.h"
#include "x86-process-inst.h"
#include "x86-unwind-analysis.h"
#include "x86-unwind-interval-fixup.h"
#include "x86-interval-arg.h"

#if defined(ENABLE_XOP) && defined (HOST_CPU_x86_64)
#include "amd-xop.h"
#endif // ENABLE_XOP and HOST_CPU_x86_64

#include <messages/messages.h>



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


extern void x86_dump_ins(void* addr);

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
    if (noisy) {
      x86_dump_ins(iarg.ins);
    }
    xed_decoded_inst_zero_keep_mode(xptr);
    xed_error = xed_decode(xptr, (uint8_t*) iarg.ins, 15);

    if (xed_error != XED_ERROR_NONE) {
#if defined(ENABLE_XOP) && defined (HOST_CPU_x86_64)
      amd_decode_t decode_res;
      adv_amd_decode(&decode_res, iarg.ins);
      if (decode_res.success) {
	if (decode_res.weak) {
	  // keep count of successes that are not robust
	}
	iarg.ins += decode_res.len;
	continue;
      }
#endif // ENABLE_XOP and HOST_CPU_x86_64
      error_count++; /* note the error      */
      iarg.ins++;         /* skip this byte      */
      continue;      /* continue onward ... */
    }

    // ensure that we don't move past the end of the interval because of a misaligned instruction
    void *nextins = iarg.ins + xed_decoded_inst_get_length(xptr);
    if (nextins > end) break;

    next = process_inst(xptr, &iarg);
    
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

// NOTE: following routine has an additional side effect!
//       the presence of tail calls is determined at interval building time,
//       (and noted in the interval where it is detected).
//
//       During the coalesce process after the initial interval set is complete,
//       the interval-specific tail call presence is OR-reduced to yield the
//       tail call presence for the entire routine.
//       The entire routine has_tail_calls value is entered in the 1st interval for the
//       routine.
//
//       The tail call info is *currently* the only routine-level info computed at interval
//       building time. If more routine-level information items come up, the reduction side
//       effect of the coalesce routine can be expanded to accomodate.

/* static */ void
x86_coalesce_unwind_intervals(unwind_interval *ui)
{
  TMSG(COALESCE,"coalescing interval list starting @ %p",ui);
  if (! ui) {
    TMSG(COALESCE,"  --interval list empty, so no work");
    return;
  }

  unwind_interval *first   = ui;
  unwind_interval *current = first;
  ui                       = (unwind_interval *) ui->common.next;
  
  bool routine_has_tail_calls = first->has_tail_calls;

  TMSG(COALESCE," starting prototype interval =");
  if (ENABLED(COALESCE)){
    dump_ui_log(current);
  }
  for (; ui; ui = (unwind_interval *)ui->common.next) {
    TMSG(COALESCE,"comparing this interval to prototype:");
    if (ENABLED(COALESCE)){
      dump_ui_log(ui);
    }
    
    routine_has_tail_calls = routine_has_tail_calls || current->has_tail_calls || ui->has_tail_calls;

    if (x86_ui_same_data(current,ui)){
      current->common.end  = ui->common.end;
      current->common.next = ui->common.next;
      current->has_tail_calls = current->has_tail_calls || ui->has_tail_calls;
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

  // update first interval with collected info
  first->has_tail_calls = routine_has_tail_calls;

  return;
}

//*************************** Debug stuff ****************************

static interval_status d_istat;

interval_status*
d_build_intervals(void* b, unsigned l)
{
  d_istat = build_intervals(b, l);
  return &d_istat;
}
