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
// Copyright ((c)) 2002-2011, Rice University
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
#include "cct2metrics.h"

#include <lush/lush-pthread.i>
#include <unwind/common/backtrace.h>

#include <lib/prof-lean/hpcio.h>

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
    exceptions package (items used when something goes wrong)
        bad_unwind
        mem_error
        handling_sample (used to distinguish a sample-based segv from a user segv)
    thread_locks package
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
       lush items can stand alone

    debug
       a few bools & integers. General purpose. Used for simulating error conditions.
 */


typedef struct thread_data_t {
  // ----------------------------------------
  // normalized thread id (monitor-generated)
  // ----------------------------------------
  int id;

  // ----------------------------------------
  // hpcrun_malloc() memory data structures
  // ----------------------------------------
  hpcrun_meminfo_t memstore;
  int              mem_low;

  // ----------------------------------------
  // sample sources
  // ----------------------------------------
  int            eventSet[MAX_POSSIBLE_SAMPLE_SOURCES];
  source_state_t ss_state[MAX_POSSIBLE_SAMPLE_SOURCES];

  uint64_t       last_time_us; // microseconds

  // ----------------------------------------
  // epoch: loadmap + cct + cct_ctxt
  // ----------------------------------------
  epoch_t* epoch;

  // ----------------------------------------
  // cct2metrics map: associate a metric_set with
  //                  a cct node
  // ----------------------------------------
  cct2metrics_t* cct2metrics_map;

  // ----------------------------------------
  // backtrace buffer
  // ----------------------------------------

  // btbuf_beg                                                  btbuf_end
  // |                                                            |
  // v low VMAs                                                   v
  // +------------------------------------------------------------+
  // [new backtrace         )              [cached backtrace      )
  // +------------------------------------------------------------+
  //                        ^              ^ 
  //                        |              |
  //                    btbuf_cur       btbuf_sav
  
  frame_t* btbuf_cur;  // current frame when actively constructing a backtrace
  frame_t* btbuf_beg;  // beginning of the backtrace buffer 
                       // also, location of the innermost frame in
                       // newly recorded backtrace (although skipInner may
                       // adjust the portion of the backtrace that is recorded)
  frame_t* btbuf_end;  // end of the current backtrace buffer
  frame_t* btbuf_sav;  // innermost frame in cached backtrace


  backtrace_t bt;     // backtrace used for unwinding

  // ----------------------------------------
  // trampoline
  // ----------------------------------------
  bool    tramp_present;   // TRUE if a trampoline installed; FALSE otherwise
  void*   tramp_retn_addr; // return address that the trampoline replaced
  void*   tramp_loc;       // current (stack) location of the trampoline
  frame_t* cached_bt;         // the latest backtrace (start)
  frame_t* cached_bt_end;     // the latest backtrace (end)
  frame_t* cached_bt_buf_end; // the end of the cached backtrace buffer
  frame_t* tramp_frame;       // (cached) frame assoc. w/ cur. trampoline loc.
  cct_node_t* tramp_cct_node; // cct node associated with the trampoline

  // ----------------------------------------
  // exception stuff
  // ----------------------------------------
  sigjmp_buf_t     bad_unwind;
  sigjmp_buf_t     mem_error;
  int              handling_sample;
  int              splay_lock;
  int              fnbounds_lock;

  // stand-alone flag to suspend sampling during some synchronous
  // calls to an hpcrun mechanism
  int              suspend_sampling;

  // ----------------------------------------
  // Logical unwinding
  // ----------------------------------------
  lushPthr_t     pthr_metrics;

  // ----------------------------------------
  // tracing
  // ----------------------------------------
  uint64_t trace_min_time_us;
  uint64_t trace_max_time_us;

  // ----------------------------------------
  // IO support
  // ----------------------------------------
  FILE* hpcrun_file;
  FILE* trace_file;
  void* trace_buffer;

  // ----------------------------------------
  // debug stuff
  // ----------------------------------------
  bool debug1;

} thread_data_t;

static const size_t HPCRUN_TraceBufferSz = HPCIO_RWBufferSz;

#define TD_GET(field) hpcrun_get_thread_data()->field

extern thread_data_t *(*hpcrun_get_thread_data)(void);
extern bool          (*hpcrun_td_avail)(void);

thread_data_t *hpcrun_allocate_thread_data(void);
void           hpcrun_init_pthread_key(void);

frame_t* hpcrun_expand_btbuf(void);
void           	hpcrun_ensure_btbuf_avail(void);
void           	hpcrun_set_thread_data(thread_data_t *td);
void           	hpcrun_set_thread0_data(void);
void           	hpcrun_unthreaded_data(void);
void           	hpcrun_threaded_data(void);

void           hpcrun_thread_data_init(int id, cct_ctxt_t* thr_ctxt, int is_child);
void           hpcrun_cached_bt_adjust_size(size_t n);

// utilities to match previous api
#define hpcrun_get_epoch()  TD_GET(epoch)

#endif // !defined(THREAD_DATA_H)
