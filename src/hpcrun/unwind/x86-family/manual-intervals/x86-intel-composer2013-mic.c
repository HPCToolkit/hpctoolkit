// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#define _GNU_SOURCE

#include <string.h>
#include "../x86-unwind-interval-fixup.h"
#include "../x86-unwind-interval.h"

static char intelmic_comp13_for_main_signature[] = {
  0x53,                         // push   %rbx
  0x48, 0x89, 0xe3,             // mov    %rsp,%rbx
  0x48, 0x83, 0xe4, 0x80,       // and    $0xffffffffffffff80,%rsp
  0x48, 0x83, 0xec, 0x78,       // sub    $0x78,%rsp
  0x55,                         // push   %rbp
  0x48, 0x8b, 0x6b, 0x08,       // mov    0x8(%rbx),%rbp
  0x48, 0x89, 0x6c, 0x24, 0x08, // mov    %rbp,0x8(%rsp)
  0x48, 0x89, 0xe5,             // mov    %rsp,%rbp
};


static char intelmic_comp13_kmp_alloc_thread_signature[] = {
  0x53,                         // push   %rbx
  0x48, 0x89, 0xe3,             // mov    %rsp,%rbx
  0x48, 0x83, 0xe4, 0xc0,       // and    $0xffffffffffffffc0,%rsp
  0x48, 0x83, 0xec, 0x38,       // sub    $0x38,%rsp
  0x55,                         // push   %rbp
  0x48, 0x8b, 0x6b, 0x08,       // mov    0x8(%rbx),%rbp
  0x48, 0x89, 0x6c, 0x24, 0x08, // mov    %rbp,0x8(%rsp)
  0x48, 0x89, 0xe5,             // mov    %rsp,%rbp
};


int
x86_adjust_intelmic_intervals(char *ins, int len, btuwi_status_t *stat)
{
  // NOTE: the two signatures above are the same length. The next three lines of code below depend upon that.
  int siglen = sizeof(intelmic_comp13_for_main_signature);
  if (len > siglen && ((strncmp((char *)intelmic_comp13_for_main_signature, ins, siglen) == 0) ||
                       (strncmp((char *)intelmic_comp13_kmp_alloc_thread_signature, ins, siglen) == 0))) {
    // signature matched
    unwind_interval *ui = stat->first;

    // this won't fix all of the intervals, but it will fix the one we care about.
    while(ui) {
       x86recipe_t *xr = UWI_RECIPE(ui);
       if (xr->ra_status == RA_STD_FRAME){
        xr->reg.bp_ra_pos = 8;
        xr->reg.bp_bp_pos = 0;
      }
      ui = UWI_NEXT(ui);
    }
    return 1;
  }
  return 0;
}
