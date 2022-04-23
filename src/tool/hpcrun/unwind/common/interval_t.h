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

#ifndef __PTRINTERVAL_T_H__
#define __PTRINTERVAL_T_H__

#include "lib/prof-lean/mem_manager.h"

#include <stdint.h>

#define MAX_INTERVAL_STR 64

typedef struct interval_s {
  uintptr_t start;
  uintptr_t end;
} interval_t;

/*
 * pre-condition: *p1 and *p2 are interval_t and do not overlap.
 * if p1->start < p2->start returns -1
 * if p1->start == p2->start returns 0
 * if p1->start > p2->start returns 1
 */
int interval_t_cmp(void* p1, void* p2);

/*
 * pre-condition:
 *   interval is a interval_t* of the form [start, end),
 *   address is uintptr_t
 * check if address is inside of interval, i.e. start <= address < end
 *
 *  // special boundary case:
 *  if (address == UINTPTR_MAX && interval.start == UINTPTR_MAX)
 *    return 0;
 *
 * if address < interval.start , i.e. interval is "greater than" address
 *   return 1
 * if interval.start <= address < interval.end, i.e. address is inside interval
 *   return 0
 * else , i.e. interval is "less than" address
 *   returns -1
 *
 */
int interval_t_inrange(void* interval, void* address);

/*
 * concrete implementation of the abstract val_tostr function of the generic_val class.
 * pre-condition: ptr_interval is of type ptrinterval_t*
 * pre-condition: result[] is an array of length >= MAX_INTERVAL_STR
 * post-condition: result[] is a string of the form [interval.start, interval.end)
 */
void interval_t_tostr(void* interval, char result[]);

void interval_t_print(interval_t* interval);

/*
 * the max spaces occupied by "([start_address ... end_address)
 */
char* interval_maxspaces();

#endif /* __PTRINTERVAL_T_H__ */
