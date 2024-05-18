// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#define _GNU_SOURCE

#include "fnbounds_interface.h"

int
fnbounds_table_lookup(void **table, int length, void *ip,
                      void **start, void **end)
{
  int lo, mid, high;
  int last = length - 1;

  if ((ip < table[0]) || (ip >= table[last])) {
    *start = 0;
    *end   = 0;
    return 1;
  }

  //---------------------------------------------------------------------
  // binary search the table for the interval containing ip
  //---------------------------------------------------------------------
  lo = 0; high = last; mid = (lo + high) >> 1;
  for(; lo != mid;) {
    if (ip >= table[mid]) lo = mid;
    else high = mid;
    mid = (lo + high) >> 1;
  }

  *start = table[lo];
  *end   = table[lo+1];
  return 0;
}
