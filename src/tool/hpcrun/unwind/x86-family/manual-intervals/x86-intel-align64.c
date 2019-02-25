// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
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

#include <string.h>
#include "x86-unwind-interval-fixup.h"
#include "x86-unwind-interval.h"

static char intel_align64_signature[] = { 
 0x53,                      	// push   %rbx
 0x48, 0x89, 0xe3,              // mov    %rsp,%rbx
 0x48, 0x83, 0xe4, 0xc0,        // and    $0xffffffffffffffc0,%rsp
 0x55,                      	// push   %rbp
 0x55,                      	// push   %rbp
 0x48, 0x8b, 0x6b, 0x08,        // mov    0x8(%rbx),%rbp
 0x48, 0x89, 0x6c, 0x24, 0x08,  // mov    %rbp,0x8(%rsp)
 0x48, 0x89, 0xe5,              // mov    %rsp,%rbp
};


int 
x86_adjust_intel_align64_intervals(char *ins, int len, btuwi_status_t *stat)
{
  int siglen = sizeof(intel_align64_signature);

  if (len > siglen && strncmp((char *)intel_align64_signature, ins, siglen) == 0) {
    // signature matched 
    unwind_interval *ui = stat->first;

    // this won't fix all of the intervals, but it will fix the ones 
    // that we care about.
    //
    // The method is as follows:
    // Ignore (do not correct) intervals before 1st std frame
    // For 1st STD_FRAME, compute the corrections for this interval and subsequent intervals
    // For this interval and subsequent interval, apply the corrected offsets
    //

    for(; UWI_RECIPE(ui)->ra_status != RA_STD_FRAME; ui = UWI_NEXT(ui));

    // this is only correct for 64-bit code
    for(; ui; ui = UWI_NEXT(ui)) {
      x86recipe_t *xr = UWI_RECIPE(ui);
      if (xr->ra_status == RA_SP_RELATIVE) continue;
      if ((xr->ra_status == RA_STD_FRAME) || (xr->ra_status == RA_BP_FRAME)) {
    	xr->ra_status = RA_BP_FRAME;
    	xr->reg.bp_ra_pos = 8;
    	xr->reg.bp_bp_pos = 0;
      }
    }

    return 1;
  } 
  return 0;
}
