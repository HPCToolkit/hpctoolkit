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
// Copyright ((c)) 2002-2018, Rice University
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
#include "x86-build-intervals.h"

#include "uw_recipe_map.h"

#include "x86-decoder.h"
#include "x86-process-inst.h"
#include "x86-unwind-analysis.h"
#include "x86-unwind-interval-fixup.h"
#include "x86-interval-arg.h"

#if defined(ENABLE_XOP) && defined (HOST_CPU_x86_64)
#include "amd-xop.h"
#endif // ENABLE_XOP and HOST_CPU_x86_64

#include <messages/messages.h>

extern void x86_dump_ins(void* addr);

static int dump_ins = 0;


/******************************************************************************
 * forward declarations 
 *****************************************************************************/

btuwi_status_t x86_build_intervals(void *ins, unsigned int len, int noisy);

static int x86_coalesce_unwind_intervals(unwind_interval *ui);

/******************************************************************************
 * interface operations 
 *****************************************************************************/

btuwi_status_t
x86_build_intervals(void *ins, unsigned int len, int noisy)
{

  btuwi_status_t status;

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
  int count          = 1;

  iarg.beg           = first_ins;
  iarg.end           = end;
  iarg.highwatermark = _h;
  iarg.ins           = ins;
  x86registers_t reg = {0, 0, BP_UNCHANGED, 0, 0};
  iarg.current       = new_ui(ins, RA_SP_RELATIVE, &reg);
  iarg.first         = iarg.current;
  iarg.restored_canonical = iarg.current;

  // handle return is different if there are any bp frames

  iarg.bp_frames_found 	     = false;
  iarg.bp_just_pushed  	     = false;
  iarg.sp_realigned  	     = false;

  iarg.rax_rbp_equivalent_at = NULL;
  iarg.canonical_interval    = NULL;

  xed_decoded_inst_zero_set_mode(xptr, &x86_decoder_settings.xed_settings);

  if (noisy) dump_ui(iarg.current, true);

  while (iarg.ins < end) {
	if (noisy && dump_ins) {
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
	void *nextins = nextInsn(&iarg, xptr);
	if (nextins > end) break;

	next = process_inst(xptr, &iarg);

	if (next != iarg.current) {
	  link_ui(iarg.current, next);
	  iarg.current = next;
	  count++;

	  if (noisy) dump_ui(iarg.current, true);
	}
	iarg.ins += xed_decoded_inst_get_length(xptr);
	UWI_END_ADDR(iarg.current) = (uintptr_t)iarg.ins;
  }

  UWI_END_ADDR(iarg.current) = (uintptr_t)end;

  status.first_undecoded_ins = iarg.ins;
  status.errcode = error_count;
  status.first   = iarg.first;

  x86_fix_unwind_intervals(iarg.beg, len, &status);
  status.count = count - x86_coalesce_unwind_intervals(status.first);

  return status;
}


/******************************************************************************
 * private operations
 *****************************************************************************/

static bool
x86_ui_same_data(x86recipe_t *proto, x86recipe_t *cand)
{
  return ( (proto->ra_status == cand->ra_status) &&
	  (proto->reg.sp_ra_pos == cand->reg.sp_ra_pos) &&
	  (proto->reg.sp_bp_pos == cand->reg.sp_bp_pos) &&
	  (proto->reg.bp_status == cand->reg.bp_status) &&
	  (proto->reg.bp_ra_pos == cand->reg.bp_ra_pos) &&
	  (proto->reg.bp_bp_pos == cand->reg.bp_bp_pos) );
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

static int
x86_coalesce_unwind_intervals(unwind_interval *ui)
{
  int num_freed = 0;
  TMSG(COALESCE,"coalescing interval list starting @ %p",ui);
  if (! ui) {
	TMSG(COALESCE,"  --interval list empty, so no work");
	return num_freed;
  }

  unwind_interval *first   = ui;
  unwind_interval *current = first;
  ui                       = UWI_NEXT(ui);

  bool routine_has_tail_calls = UWI_RECIPE(first)->has_tail_calls;

  TMSG(COALESCE," starting prototype interval =");
  if (ENABLED(COALESCE)){
	dump_ui_log(current);
  }
  for (; ui; ui = UWI_NEXT(ui)) {
	TMSG(COALESCE,"comparing this interval to prototype:");
	if (ENABLED(COALESCE)){
	  dump_ui_log(ui);
	}

	routine_has_tail_calls =
		routine_has_tail_calls || UWI_RECIPE(current)->has_tail_calls || UWI_RECIPE(ui)->has_tail_calls;

	if (x86_ui_same_data(UWI_RECIPE(current), UWI_RECIPE(ui))){
	  UWI_END_ADDR(current) = UWI_END_ADDR(ui);
	  bitree_uwi_set_rightsubtree(current, UWI_NEXT(ui));
	  UWI_RECIPE(current)->has_tail_calls =
		  UWI_RECIPE(current)->has_tail_calls || UWI_RECIPE(ui)->has_tail_calls;
	  // disconnect ui's right subtree and free ui:
	  bitree_uwi_set_rightsubtree(ui, NULL);
	  bitree_uwi_free(NATIVE_UNWINDER, ui);
	  ++num_freed;

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
  UWI_RECIPE(first)->has_tail_calls = routine_has_tail_calls;

  return num_freed;
}

//*************************** Debug stuff ****************************

static btuwi_status_t d_istat;

btuwi_status_t*
d_build_intervals(void* b, unsigned l)
{
  d_istat = x86_build_intervals(b, l, 1);
  return &d_istat;
}
