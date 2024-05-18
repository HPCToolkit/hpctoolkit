// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#define _GNU_SOURCE

#include <string.h>
#include "../x86-unwind-interval-fixup.h"
#include "../x86-unwind-interval.h"


static char main32_signature[] = {

  0x8d, 0x4c, 0x24, 0x04, // lea    0x4(%esp),%ecx
  0x83, 0xe4, 0xf0,       // and    $0xfffffff0,%esp
  0xff, 0x71, 0xfc,       // pushl  -0x4(%ecx)
};


int
x86_adjust_32bit_main_intervals(char *ins, int len, btuwi_status_t *stat)
{
  int siglen = sizeof(main32_signature);

  if (len > siglen && strncmp((char *)main32_signature, ins, siglen) == 0) {
    // signature matched
    unwind_interval *ui = stat->first;

    // this won't fix all of the intervals, but it will fix the ones
    // that we care about.
    while(ui) {
      x86recipe_t *xr = UWI_RECIPE(ui);
      if (xr->ra_status == RA_STD_FRAME){
        xr->reg.bp_ra_pos = 4;
        xr->reg.bp_bp_pos = 0;
        xr->reg.sp_ra_pos = 4;
        xr->reg.sp_bp_pos = 0;
      }
      ui = UWI_NEXT(ui);
    }
    return 1;
  }
  return 0;
}
