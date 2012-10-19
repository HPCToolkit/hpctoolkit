// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://outreach.scidac.gov/svn/hpctoolkit/trunk/src/tool/hpcrun/thread_data.h $
// $Id: thread_data.h 3956 2012-10-11 06:41:18Z krentel $
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2012, Rice University
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


#ifndef CPU_DATA_H
#define CPU_DATA_H

// This is called "thread data", but it applies to process as well
// (there is just 1 thread).

#include <setjmp.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#include "sample_sources_registered.h"
#include "newmem.h"
#include "epoch.h"
#include "cct2metrics.h"

#include <lush/lush-pthread.i>
#include <unwind/common/backtrace.h>

#include <lib/prof-lean/hpcio.h>
#include <lib/prof-lean/hpcio-buffer.h>

/* ******
 * cpu_data should have the following fields:
 *
 * epoch: each cpu has one epoch
 *
 * cct2metrics_map
 *
 * trace facilities
 */


typedef struct cpu_data_t {
  // ----------------------------------------
  // normalized thread id (monitor-generated)
  // ----------------------------------------
  int id;

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

#if 0
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
  size_t  cached_frame_count; // (sanity check) length of cached frame list
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

  // ----------------------------------------
  // Logical unwinding
  // ----------------------------------------
  lushPthr_t     pthr_metrics;
#endif

  // ----------------------------------------
  // tracing
  // ----------------------------------------
  uint64_t trace_min_time_us;
  uint64_t trace_max_time_us;

  // ----------------------------------------
  // IO support
  // ----------------------------------------
  FILE* hpcrun_file;
  void* trace_buffer;
  hpcio_outbuf_t trace_outbuf;

#if 0
  // ----------------------------------------
  // debug stuff
  // ----------------------------------------
  bool debug1;

  // ----------------------------------------
  // miscellaneous
  // ----------------------------------------
  // Set to 1 while inside hpcrun code for safe sampling.
  int inside_hpcrun;

  // True if this thread is inside dlopen or dlclose.  A synchronous
  // override that is called from dlopen (eg, malloc) must skip this
  // sample or else deadlock on the dlopen lock.
  bool inside_dlfcn;
#endif

} cpu_data_t;

void hpcrun_cpu_data_init(int cpu);
void hpcrun_allocate_cpu_data(int cpu);
cpu_data_t *hpcrun_get_cpu_data(int cpu);
int hpcrun_get_max_cpu();

void hpcrun_init_cpu_trace_lock();
void hpcrun_set_cpu_trace_lock(int cpu);
void hpcrun_unset_cpu_trace_lock(int cpu);
#endif // !defined(THREAD_DATA_H)
