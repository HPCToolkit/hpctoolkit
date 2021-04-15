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
// Copyright ((c)) 2002-2021, Rice University
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

#include "logical.h"

#include "cct_backtrace_finalize.h"
#include "hpcrun-malloc.h"
#include "messages/messages.h"
#include "thread_data.h"

static void logicalize_bt(backtrace_info_t*, int);

// ---------------------------------------
// Logical stack mutation functions
// ---------------------------------------

#define REGIONS_PER_SEGMENT \
  (sizeof( ((struct logical_region_segment_t*)0)->regions ) \
   / sizeof( ((struct logical_region_segment_t*)0)->regions[0] ))

void hpcrun_logical_stack_init(logical_region_stack_t* s) {
  s->depth = 0;
  s->head = NULL;
  s->spare = NULL;
}

logical_region_t* hpcrun_logical_stack_top(logical_region_stack_t* s) {
  if(s->depth == 0) return NULL;
  return &s->head->regions[(s->depth-1) % REGIONS_PER_SEGMENT];
}

logical_region_t* hpcrun_logical_stack_push(logical_region_stack_t* s,
                                            const logical_region_t* r) {
  // Figure out where the next element goes, allocate some space if we must
  if(s->depth % REGIONS_PER_SEGMENT == 0) {
    if(s->spare != NULL) {
      // Pop from spare for the next segment
      struct logical_region_segment_t* newhead = s->spare;
      s->spare = newhead->prev;
      newhead->prev = s->head;
      s->head = newhead;
    } else {
      // Allocate a new entry for the next segment
      struct logical_region_segment_t* newhead = hpcrun_malloc(sizeof *newhead);
      newhead->prev = s->head;
      s->head = newhead;
    }
  }
  logical_region_t* next = &s->head->regions[s->depth % REGIONS_PER_SEGMENT];

  // Copy over the region data, and return the new top
  memcpy(next, r, sizeof *next);
  return next;
}

size_t hpcrun_logical_stack_settop(logical_region_stack_t* s, size_t n) {
  if(n >= s->depth) return n - s->depth;
  // Pop off segments until we've got the high order bits
  size_t n_segs = n / REGIONS_PER_SEGMENT;
  for(; s->depth / REGIONS_PER_SEGMENT > n_segs; s->depth -= REGIONS_PER_SEGMENT) {
    struct logical_region_segment_t* newhead = s->head->prev;
    s->head->prev = s->spare;
    s->spare = s->head;
    s->head = newhead;
  }
  // We're within the right segment, so we can just set the depth now.
  s->depth = n;
  return 0;
}

// ---------------------------------------
// Backtrace modification engine
// ---------------------------------------

static bool logicalize_bt_registered = false;
static cct_backtrace_finalize_entry_t logicalize_bt_entry = {logicalize_bt};
void hpcrun_logical_register() {
  if(!logicalize_bt_registered) {
    cct_backtrace_finalize_register(&logicalize_bt_entry);
    logicalize_bt_registered = true;
  }
}

static void logicalize_bt(backtrace_info_t* bt, int isSync) {
  thread_data_t* td = hpcrun_get_thread_data();
  if(td->logical.depth == 0) return;  // No need for our services

  ETMSG(LOGICAL_CTX, "Logicalization routine called, logical depth = %d", td->logical.depth);
}
