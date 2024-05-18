// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

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

#include "../../utilities/ip-normalized.h"
#include "unw-datatypes.h"
#include "backtrace_info.h"
#include "../../frame.h"

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

frame_t* hpcrun_skip_chords(frame_t* bt_outer, frame_t* bt_inner,
                            int skip);

void hpcrun_bt_dump(frame_t* unwind, const char* tag);

void     hpcrun_bt_init(backtrace_t* bt, size_t size);

void hpcrun_backtrace_setup();

bool     hpcrun_backtrace_std(backtrace_t* bt, ucontext_t* context);

bool hpcrun_generate_backtrace(backtrace_info_t* bt,
                               ucontext_t* context, int skipInner);

bool hpcrun_generate_backtrace_no_trampoline(backtrace_info_t* bt,
                                             ucontext_t* context, int skipInner);

extern bool hpcrun_no_unwind;

#endif // hpcrun_backtrace_h
