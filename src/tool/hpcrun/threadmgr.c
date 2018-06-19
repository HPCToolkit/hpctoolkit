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
// Copyright ((c)) 2002-2018, Rice University
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

//******************************************************************************
// File: threadmgr.c: 
// Purpose: maintain information about the number of live threads
//******************************************************************************



//******************************************************************************
// system include files 
//******************************************************************************
#include <stdint.h>



//******************************************************************************
// local include files 
//******************************************************************************
#include "threadmgr.h"
#include "thread_data.h"
#include "write_data.h"
#include "trace.h"

#include <lib/prof-lean/stdatomic.h>
#include <lib/prof-lean/spinlock.h>


#include <include/queue.h>

//******************************************************************************
// data structure
//******************************************************************************

typedef struct thread_list_s {
  thread_data_t *thread_data;
  SLIST_ENTRY(thread_list_s) entries;
} thread_list_t;

//******************************************************************************
// private data
//******************************************************************************
static atomic_int_least32_t threadmgr_active_threads = ATOMIC_VAR_INIT(1); // one for the process main thread

static SLIST_HEAD(thread_list_head, thread_list_s) list_thread_head =
    SLIST_HEAD_INITIALIZER(thread_list_head);

static spinlock_t threaddata_lock = SPINLOCK_UNLOCKED;

//******************************************************************************
// private operations
//******************************************************************************

static void
adjust_thread_count(int32_t val)
{
	atomic_fetch_add_explicit(&threadmgr_active_threads, val, memory_order_relaxed);
}



//******************************************************************************
// interface operations
//******************************************************************************

void
hpcrun_threadmgr_thread_new()
{
	adjust_thread_count(1);
}


void 
hpcrun_threadmgr_thread_delete()
{
	adjust_thread_count(-1);
}


int 
hpcrun_threadmgr_thread_count()
{
	return atomic_load_explicit(&threadmgr_active_threads, memory_order_relaxed);
}


thread_data_t *
hpcrun_threadMgr_data_get(int id, cct_ctxt_t* thr_ctxt, size_t num_sources )
{
  thread_data_t *data = NULL;

  spinlock_lock(&threaddata_lock);
  if (SLIST_EMPTY(&list_thread_head)) {

    spinlock_unlock(&threaddata_lock);

    data = hpcrun_allocate_thread_data(id);

    // requires to set the data before calling thread_data_init
    // since the function will query the thread local data :-(

    hpcrun_set_thread_data(data);
    hpcrun_thread_data_init(id, thr_ctxt, 0, num_sources);

  } else {
    struct thread_list_s *item = SLIST_FIRST(&list_thread_head);
    spinlock_unlock(&threaddata_lock);

    data = item->thread_data;
  }

  return data;
}


void
hpcrun_threadMgr_data_put( thread_data_t *data )
{
  spinlock_lock(&threaddata_lock);

  thread_list_t *list_item = (thread_list_t *) hpcrun_malloc(sizeof(thread_list_t));
  list_item->thread_data   = data;
  SLIST_INSERT_HEAD(&list_thread_head, list_item, entries);


  spinlock_unlock(&threaddata_lock);
}


void
hpcrun_threadMgr_data_fini(core_profile_trace_data_t *current_data)
{
  thread_list_t *item = NULL;
  bool is_processed   = false;

  while( !SLIST_EMPTY(&list_thread_head) ) {
    item = SLIST_FIRST(&list_thread_head);
    if (item != NULL) {
      core_profile_trace_data_t *data = &item->thread_data->core_profile_trace_data;

      hpcrun_write_profile_data( data );
      hpcrun_trace_close( data );

      if (data == current_data) {
        is_processed = true;
      }
      SLIST_REMOVE_HEAD(&list_thread_head, entries);
    }
  }
  // main thread (thread 0) may not be in the list
  // for sequential or pure MPI programs, they don't have list of thread data in this queue.
  // hence, we need to process specifically here
  if (!is_processed) {
    hpcrun_write_profile_data( current_data );
    hpcrun_trace_close( current_data );
  }
}


