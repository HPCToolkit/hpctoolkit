// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#define _GNU_SOURCE

#include <stdint.h>
#include <string.h>
#include "../x86-unwind-interval-fixup.h"
#include "../x86-unwind-interval.h"


static char icc_variant_signature[] = {
  0x53,                   // push %ebx
  0x8b, 0xdc,             // mov  %esp,%ebx
  0x83,0xe4,0xe0,         // and  $0xffffffe0,%esp
  0x55,                   // push %ebp
  0x55,                   // push %ebp
  0x8b, 0x6b, 0x04,       // mov  0x4(%ebx),%ebp
  0x89, 0x6c, 0x24, 0x04, // mov  %ebp,0x4(%esp)
  0x8b, 0xec,             // mov  %esp,%ebp
};


int
x86_adjust_icc_variant_intervals(char *ins, int len, btuwi_status_t* stat)
{
  int siglen = sizeof(icc_variant_signature);

  if (len > siglen && strncmp((char *)icc_variant_signature, ins, siglen) == 0) {
    // signature matched
    unwind_interval* ui = (unwind_interval *) stat->first;

    // this won't fix all of the intervals, but it will fix the ones
    // that we care about.
    //
    // The method is as follows:
    // Ignore (do not correct) intervals before 1st std frame
    // For 1st STD_FRAME, compute the corrections for this interval and subsequent intervals
    // For this interval and subsequent interval, apply the corrected offsets
    //

    for(;  UWI_RECIPE(ui)->ra_status != RA_STD_FRAME; ui = UWI_NEXT(ui));

    x86recipe_t *xr = UWI_RECIPE(ui);
    int ra_correction =  xr->reg.sp_ra_pos - 4; // N.B. The '4' is only correct for 32 bit
    int bp_correction =  xr->reg.sp_bp_pos;

    for(; ui; ui = UWI_NEXT(ui)) {
      xr = UWI_RECIPE(ui);
      xr->reg.bp_ra_pos -= ra_correction;
      xr->reg.bp_bp_pos -= bp_correction;
      xr->reg.sp_ra_pos -= ra_correction;
      xr->reg.sp_bp_pos -= bp_correction;
    }
    return 1;
  }
  return 0;
}
