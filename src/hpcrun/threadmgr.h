// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//******************************************************************************
// File: threadmgr.h
//
// Purpose:
//   interface definitions for threadmgr, which maintains information
//   about the number of live threads
//******************************************************************************

#ifndef _threadmgr_h_
#define _threadmgr_h_

#include "thread_data.h"
#include "trace.h"


//******************************************************************************
// constants
//******************************************************************************

#define OPTION_NO_COMPACT_THREAD  0
#define OPTION_COMPACT_THREAD     1

//******************************************************************************
// interface operations
//******************************************************************************

void hpcrun_threadmgr_thread_new();

void hpcrun_threadmgr_thread_delete();

int hpcrun_threadmgr_thread_count();

bool
hpcrun_threadMgr_data_get(int id, cct_ctxt_t* thr_ctxt, thread_data_t **data, hpcrun_trace_type_t trace, bool demand_new_thread);

void
hpcrun_threadMgr_non_compact_data_get(int id, cct_ctxt_t* thr_ctxt, thread_data_t **data, hpcrun_trace_type_t trace);

void
hpcrun_threadMgr_data_put(epoch_t *epoch, thread_data_t *data, bool add_separator);

void
hpcrun_threadMgr_data_fini(thread_data_t *td);

int
hpcrun_threadMgr_compact_thread();

#endif
