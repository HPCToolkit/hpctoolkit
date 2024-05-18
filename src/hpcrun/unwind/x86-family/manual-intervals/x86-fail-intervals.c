// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#define _GNU_SOURCE

#include <string.h>
#include "../x86-unwind-interval-fixup.h"
#include "../x86-unwind-interval.h"

#define X86_FAIL_INTERVALS_DEBUG 1

static char fail_signature[] = {
  0xc3,                         // retq
  0xc3,                         // retq
  0xc3,                         // retq
  0xc3,                         // retq
  0xc3                          // retq
};


int
x86_fail_intervals(char *ins, int len, btuwi_status_t *stat)
{
#ifdef X86_FAIL_INTERVALS_DEBUG
  int siglen = sizeof(fail_signature);

  if (len > siglen && strncmp((char *) fail_signature, ins, siglen) == 0) {
    // signature matched
    char *null = 0;

    *null = 0; // cause a SEGV by storing to the null pointer
  }

#endif
  return 0;
}
