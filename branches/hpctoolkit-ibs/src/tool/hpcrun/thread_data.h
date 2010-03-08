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
// Copyright ((c)) 2002-2010, Rice University 
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
#include "epoch.h"

#include <lush/lush-pthread.i>
#include <unwind/common/backtrace.h>

typedef struct {
  sigjmp_buf jb;
} sigjmp_buf_t;


/* ******
   TODO:

   thread_data_t needs more structure.
   Organize according to following plan:

   backtrace_buffer
     factor out of the current epoch, and put it as a separate item in thread_data
     it will become an opaque datatype
   each epoch has:
       loadmap   (currently called epoch)
       cct       (the main sample data container)
       cct_ctxt  (pthread creation context)
    exceptions package (items used when somethign goes wrong)
        bad_unwind
        mem_error
        handling_sample (used to distinguish a sample-based segv from a user segv)
        (each of the 'lock' elements is true if this thread owns the lock.
         locks must be released in an exceptional situation)
	fnbounds_lock   
	splay_lock
    sample source package
       event_set
       ss_state
    trampoline
       all trampoline-related items should be collected into a struct
       NOTE: a single backtrace_buffer item will replace the collection of fields
             that currently implement the cached backtrace.
    io_support
       This is a collection of files:
         trace_file
         hpcrun_file

    suspend_sample
       Stands on its own

    lushPthr_t
       lush items can stand along
 */


typedef struct thread_data_t {
  // monitor-generated thread id
  int id;

  // hpcrun_malloc memory data structures

  hpcrun_meminfo_t memstore;
  int              mem_low;

  // backtrace buffer

  // btbuf                                                      bufend
  // |                                                            |
  // v low VMAs                                                   v
  // +------------------------------------------------------------+
  // [new backtrace         )              [cached backtrace      )
  // +------------------------------------------------------------+
  //                        ^              ^ 
  //                        |              |
  //                      unwind         bufstk
  
  frame_t*       unwind;    // current frame
  frame_t*       btbuf; 	   // innermost frame in new backtrace
  frame_t*       bufend;	   // 
  frame_t*       bufstk;	   // innermost frame in cached backtrace

  backtrace_t    bt;        // backtrace used for unwinding

  // the loadmap + cct + cct_ctxt = epoch
  epoch_t*         epoch;

  // exception stuff
  sigjmp_buf_t     bad_unwind;
  sigjmp_buf_t     mem_error;
  int              handling_sample;
  int              splay_lock;
  int              fnbounds_lock;

  // sample sources
  int              eventSet[MAX_POSSIBLE_SAMPLE_SOURCES];
  source_state_t   ss_state[MAX_POSSIBLE_SAMPLE_SOURCES];

  // stand-alone flag to suspend sampling during some synchronous calls to an hpcrun
  // mechanism
  //
  int              suspend_sampling;
  //
  // trampoline related storage
  //
  bool               tramp_present;      // TRUE if there is a trampoline installed, FALSE otherwise
  void*              tramp_retn_addr;    // the return address that the trampoline replaced
  void*              tramp_loc;          // current (stack) location of the trampoline
  frame_t*    cached_bt;          // the latest backtrace (start)
  frame_t*    cached_bt_end;      // the latest backtrace (end)
  frame_t*    cached_bt_buf_end;  // the end of the cached backtrace buffer
  frame_t*    tramp_frame;        // (cached) frame associated with current trampoline location
  cct_node_t* tramp_cct_node;     //  cct node associated with the trampoline

  // IO support
  FILE*            hpcrun_file;
  FILE*            trace_file;

  uint64_t         last_time_us; // microseconds

  lushPthr_t      pthr_metrics;
} thread_data_t;

#define TD_GET(field) hpcrun_get_thread_data()->field

thread_data_t *(*hpcrun_get_thread_data)(void);
bool          (*hpcrun_td_avail)(void);
thread_data_t *hpcrun_allocate_thread_data(void);
void           hpcrun_init_pthread_key(void);

frame_t* hpcrun_expand_btbuf(void);
void           	hpcrun_ensure_btbuf_avail(void);
void           	hpcrun_set_thread_data(thread_data_t *td);
void           	hpcrun_set_thread0_data(void);
void           	hpcrun_unthreaded_data(void);
void           	hpcrun_threaded_data(void);

thread_data_t* hpcrun_thread_data_new(void);
void           hpcrun_thread_memory_init(void);
void           hpcrun_thread_data_init(int id, cct_ctxt_t* thr_ctxt);
void           hpcrun_cached_bt_adjust_size(size_t n);

// utilities to match previous api
#define hpcrun_get_epoch()  TD_GET(epoch)

#endif // !defined(THREAD_DATA_H)
