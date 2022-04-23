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
// Copyright ((c)) 2002-2022, Rice University
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

#include "interval_t.h"

#include "lib/prof-lean/mcs-lock.h"

#include <stdio.h>
#include <stdlib.h>

static char Interval_MaxSpaces[] = "                                               ";

int interval_t_cmp(void* lhs, void* rhs) {
  interval_t* p1 = (interval_t*)lhs;
  interval_t* p2 = (interval_t*)rhs;
  if (p1->start < p2->start)
    return -1;
  if (p1->start == p2->start)
    return 0;
  return 1;
}

int interval_t_inrange(void* lhs, void* val) {
  interval_t* interval = (interval_t*)lhs;
  uintptr_t address = (uintptr_t)val;
  // special boundary case:
  if (address == UINTPTR_MAX && interval->start == UINTPTR_MAX) {
    return 0;
  } else {
    return (address < interval->start) ? 1 : (address < interval->end) ? 0 : -1;
  }
}

/*
 * pre-condition: result[] is an array of length >= MAX_PTRINTERVAL_STR
 * post-condition: a string of the form [ptrinterval->start, ptrinterval->end)
 */
void interval_t_tostr(void* ptrinterval, char result[]) {
  interval_t* ptr_interval = (interval_t*)ptrinterval;
  snprintf(
      result, MAX_INTERVAL_STR, "%s%18p%s%18p%s", "[", (void*)ptr_interval->start, " ... ",
      (void*)ptr_interval->end, ")");
}

void interval_t_print(interval_t* ptrinterval) {
  char ptrinterval_buff[MAX_INTERVAL_STR];
  interval_t_tostr(ptrinterval, ptrinterval_buff);
  printf("%s", ptrinterval_buff);
}

char* interval_maxspaces() {
  return Interval_MaxSpaces;
}
