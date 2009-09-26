// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2009, Rice University 
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

//
//

#ifndef THREAD_DATA_H
#define THREAD_DATA_H

// This is called "thread data", but it applies to process as well
// (there is just 1 thread).

#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "sample_sources_registered.h"
#include "newmem.h"
#include "state.h"

#include <lush/lush-pthread.i>

typedef struct {
  sigjmp_buf jb;
} sigjmp_buf_t;


typedef struct _td_t {
  int id;
  hpcrun_meminfo_t memstore;
  int              mem_low;
  state_t*         state;
  FILE*            hpcrun_file;
  sigjmp_buf_t     bad_unwind;
  sigjmp_buf_t     mem_error;
  int              eventSet[MAX_POSSIBLE_SAMPLE_SOURCES];
  source_state_t   ss_state[MAX_POSSIBLE_SAMPLE_SOURCES];
  int              handling_sample;
  int              splay_lock;
  int              fnbounds_lock;
  int              suspend_sampling;
  //
  // trampoline related storage
  //
  bool               tramp_present;      // TRUE if there is a trampoline installed, FALSE otherwise
  void*              tramp_retn_addr;    // the return address that the trampoline replaced
  void*              tramp_loc;          // current (stack) location of the trampoline
  hpcrun_frame_t*    cached_bt;          // the latest backtrace (start)
  hpcrun_frame_t*    cached_bt_end;      // the latest backtrace (end)
  hpcrun_frame_t*    cached_bt_buf_end;  // the end of the cached backtrace buffer
  hpcrun_frame_t*    tramp_frame;        // (cached) frame associated with current trampoline location
  csprof_cct_node_t* tramp_cct_node;     // cct node associated with the trampoline

  FILE*            trace_file;
  uint64_t         last_time_us; // microseconds

  lushPthr_t      pthr_metrics;
} thread_data_t;

#define TD_GET(field) hpcrun_get_thread_data()->field

thread_data_t *(*hpcrun_get_thread_data)(void);
bool          (*hpcrun_td_avail)(void);
thread_data_t *hpcrun_allocate_thread_data(void);
void           hpcrun_init_pthread_key(void);

void           hpcrun_set_thread_data(thread_data_t *td);
void           hpcrun_set_thread0_data(void);
void           hpcrun_unthreaded_data(void);
void           hpcrun_threaded_data(void);

thread_data_t* hpcrun_thread_data_new(void);
void           hpcrun_thread_memory_init(void);
void           hpcrun_thread_data_init(int id, lush_cct_ctxt_t* thr_ctxt);
void           hpcrun_cached_bt_adjust_size(size_t n);

// utilities to match previous api
#define csprof_get_state()  TD_GET(state)

#endif // !defined(THREAD_DATA_H)
