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
// Copyright ((c)) 2002-2016, Rice University
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

#ifndef hpcrun_backtrace_h
#define hpcrun_backtrace_h

//***************************************************************************
// file: backtrace.h
//
// purpose:
//     an interface for performing stack unwinding 
//
//***************************************************************************


//***************************************************************************
// system include files 
//***************************************************************************

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <ucontext.h>

//***************************************************************************
// local include files 
//***************************************************************************

#include <hpcrun/utilities/ip-normalized.h>
#include <unwind/common/unw-datatypes.h>
#include <unwind/common/backtrace_info.h>
#include <hpcrun/frame.h>

//
// backtrace_t type is a struct holding begin, end, current ptrs
//  to frames
//

typedef struct backtrace_t {
  size_t   size;   // size of backtrace
  size_t   len;    // # of frames used
  frame_t* beg;    // base memory chunk
  frame_t* end;    // the last available slot
  frame_t* cur;    // current insertion position
} backtrace_t;

typedef struct bt_iter_t {
  frame_t* cur;
  backtrace_t* bt;
} bt_iter_t;

//***************************************************************************
// interface functions
//***************************************************************************

typedef bool bt_fn(backtrace_t* bt, ucontext_t* context);
typedef void* bt_fn_arg;
typedef void (*bt_mut_fn)(backtrace_t* bt, bt_fn_arg arg);

bool hpcrun_backtrace_std(backtrace_t* bt, ucontext_t* context);

bool hpcrun_filter_sample(int len, frame_t* start, frame_t* last);

frame_t* hpcrun_skip_chords(frame_t* bt_outer, frame_t* bt_inner, 
			    int skip);

void hpcrun_bt_dump(frame_t* unwind, const char* tag);

frame_t* hpcrun_bt_reset(backtrace_t* bt);

void     hpcrun_bt_init(backtrace_t* bt, size_t size);

frame_t* hpcrun_bt_push(backtrace_t* bt, frame_t* frame);

frame_t* hpcrun_bt_beg(backtrace_t* bt);

frame_t* hpcrun_bt_last(backtrace_t* bt);

frame_t* hpcrun_bt_cur(backtrace_t* bt);

size_t   hpcrun_bt_len(backtrace_t* bt);

bool     hpcrun_bt_empty(backtrace_t* bt);

bool     hpcrun_backtrace_std(backtrace_t* bt, ucontext_t* context);

void hpcrun_bt_modify_leaf_addr(backtrace_t* bt, ip_normalized_t ip_norm);

void hpcrun_bt_add_leaf_child(backtrace_t* bt, ip_normalized_t ip_norm);

void hpcrun_dump_bt(backtrace_t* bt);

bool hpcrun_generate_backtrace(backtrace_info_t* bt,
			       ucontext_t* context, int skipInner);

bool hpcrun_generate_backtrace_no_trampoline(backtrace_info_t* bt,
					     ucontext_t* context, int skipInner);

bool hpcrun_dbg_generate_backtrace(backtrace_info_t* bt,
			       ucontext_t* context, int skipInner);

bool hpcrun_gen_bt(ucontext_t* context, bool* has_tramp, bt_mut_fn bt_fn, bt_fn_arg bt_arg);

#endif // hpcrun_backtrace_h
