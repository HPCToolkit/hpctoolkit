// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#define _GNU_SOURCE

#include <string.h>
#include "../x86-unwind-interval-fixup.h"
#include "../x86-unwind-interval.h"

static char intel_align64_signature[] = {
 0x53,                          // push   %rbx
 0x48, 0x89, 0xe3,              // mov    %rsp,%rbx
 0x48, 0x83, 0xe4, 0xc0,        // and    $0xffffffffffffffc0,%rsp
 0x55,                          // push   %rbp
 0x55,                          // push   %rbp
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
