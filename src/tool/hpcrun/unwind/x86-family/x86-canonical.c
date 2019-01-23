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
// Copyright ((c)) 2002-2019, Rice University
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

#include "x86-interval-highwatermark.h"
#include "x86-decoder.h"
#include "x86-unwind-interval.h"
#include "x86-interval-arg.h"

#include <messages/messages.h>

#define HW_INITIALIZED          0x8
#define HW_BP_SAVED             0x4
#define HW_BP_OVERWRITTEN       0x2
#define HW_SP_DECREMENTED       0x1
#define HW_UNINITIALIZED        0x0


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
reset_to_canonical_interval(xed_decoded_inst_t *xptr, unwind_interval **next,
	bool irdebug, interval_arg_t *iarg)
{
  unwind_interval *current             = iarg->current;
  unwind_interval *first               = iarg->first;
  unwind_interval *hw_uwi              = iarg->highwatermark.uwi;

  // if the return is not the last instruction in the interval, 
  // set up an interval for code after the return 
  if ((void*)nextInsn(iarg, xptr) < iarg->end) {
    if (iarg->bp_frames_found) { 
      // look for first bp frame
      first = find_first_bp_frame(first);
      set_ui_canonical(first, iarg->canonical_interval);
      iarg->canonical_interval = first;
    } else if (iarg->canonical_interval) {
      x86recipe_t *xr = hw_uwi ? UWI_RECIPE(hw_uwi) : NULL;
      if (xr && xr->reg.bp_status != BP_UNCHANGED) {
	bp_loc bp_status = UWI_RECIPE(iarg->canonical_interval)->reg.bp_status;
	if ((bp_status == BP_UNCHANGED) ||
	    ((bp_status == BP_SAVED) &&
             (xr->reg.bp_status == BP_HOSED))) {
         set_ui_canonical(hw_uwi, iarg->canonical_interval);
         iarg->canonical_interval = hw_uwi;
	}
      }
      first = iarg->canonical_interval;
    } else { 
      // look for first nondecreasing with no jmp
      first = find_first_non_decr(first, hw_uwi);
      set_ui_canonical(first, iarg->canonical_interval);
      iarg->canonical_interval = first;
    }
    {
      x86recipe_t *xr = UWI_RECIPE(current);
      x86recipe_t *r1 = UWI_RECIPE(first);
      x86registers_t reg = r1->reg;
      ra_loc ra_status = r1->ra_status;
      bp_loc bp_status =
    	  (xr->reg.bp_status == BP_HOSED) ? BP_HOSED : reg.bp_status;
#ifndef FIX_INTERVALS_AT_RETURN
      if (xr->ra_status != ra_status) ||
	  xr->reg.bp_status != bp_status) ||
	  xr->reg.sp_ra_pos != reg.sp_ra_pos) ||
	  xr->reg.bp_ra_pos != reg.bp_ra_pos) ||
	  xr->reg.bp_bp_pos != reg.bp_bp_pos) ||
	  xr->reg.sp_bp_pos != reg.sp_bp_pos))
#endif
	{
	  reg.bp_status = bp_status;
	*next = new_ui(nextInsn(iarg, xptr), ra_status, &reg);
	iarg->restored_canonical = *next;
        set_ui_canonical(*next, UWI_RECIPE(iarg->canonical_interval)->prev_canonical);
        if (r1->reg.bp_status != BP_HOSED && bp_status == BP_HOSED) {
          set_ui_canonical(*next, iarg->canonical_interval);
          iarg->canonical_interval = *next;
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
  while (first && (UWI_RECIPE(first)->ra_status != RA_BP_FRAME))
	first = UWI_NEXT(first);
  return first;
}

unwind_interval *
find_first_non_decr(unwind_interval *first, unwind_interval *highwatermark)
{
  if (first == NULL)
    return NULL;
  int sp_ra_pos = UWI_RECIPE(first)->reg.sp_ra_pos;
  int next_ra_pos;
  unwind_interval *next;
  while ((next = UWI_NEXT(first)) &&
	 (sp_ra_pos <= (next_ra_pos = UWI_RECIPE(next)->reg.sp_ra_pos)) &&
	 (first != highwatermark)) {
    first = next;
    sp_ra_pos = next_ra_pos;
  }
  return first;
}

