// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#define _GNU_SOURCE

#include <string.h>
#include "../x86-unwind-interval-fixup.h"
#include "../x86-unwind-interval.h"

static char pgi_mp_pexit_signature[] = {
  0x41,  0x5b,                                  // pop    %r11
  0x4c, 0x89, 0x9c, 0x24, 0xb0, 0x00, 0x00,     // mov    %r11,0xb0(%rsp)
  0x00,
  0x48, 0x83, 0x7c, 0x24, 0x08, 0x01,           // cmpq   $0x1,0x8(%rsp)
};


int
x86_adjust_pgi_mp_pexit_intervals(char *ins, int len, btuwi_status_t *stat)
{
  int siglen = sizeof(pgi_mp_pexit_signature);

  if (len > siglen && strncmp((char *)pgi_mp_pexit_signature, ins, siglen) == 0) {
    // signature matched
    unwind_interval *ui = stat->first;

    // this won't fix all of the intervals, but it will fix the one we care about.
    while(ui) {
      x86recipe_t *xr = UWI_RECIPE(ui);
      if (xr->ra_status == RA_SP_RELATIVE){
        xr->reg.sp_ra_pos = 0xb0;
        xr->reg.sp_bp_pos = 0;
      }
      ui = UWI_NEXT(ui);
    }
    return 1;
  }
  return 0;
}
