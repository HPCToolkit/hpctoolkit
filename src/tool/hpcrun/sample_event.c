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


#include <setjmp.h>
#include <string.h>

//*************************** User Include Files ****************************

#include "backtrace.h"
#include "cct.h"
#include "csprof_dlfns.h"
#include "csprof-malloc.h"
#include "fnbounds_interface.h"
#include "main.h"
#include "metrics_types.h"
#include "segv_handler.h"
#include "state.h"
#include "thread_data.h"
#include "trace.h"
#include "handling_sample.h"
#include "interval-interface.h"
#include "unwind.h"
#include "csprof-malloc.h"
#include "splay-interval.h"
#include "sample_event.h"
#include "sample_sources_all.h"
#include "ui_tree.h"
#include "validate_return_addr.h"
#include "write_data.h"

#include <messages/messages.h>

#include <lib/prof-lean/atomic-op.h>
#include <lib/prof-lean/hpcrun-fmt.h>


//*************************** Forward Declarations **************************

static cct_node_t*
_hpcrun_sample_callpath(state_t *state, void *context,
			int metricId, uint64_t metricIncr,
			int skipInner, int isSync);

//***************************************************************************

static long num_samples_total = 0;
static long num_samples_attempted = 0;
static long num_samples_blocked_async = 0;
static long num_samples_blocked_dlopen = 0;
static long num_samples_dropped = 0;
static long num_samples_filtered = 0;

static int _sampling_disabled = 0;


int
sampling_is_disabled(void)
{
  return _sampling_disabled;
}

void
csprof_disable_sampling(void)
{
  TMSG(SPECIAL,"Sampling disabled");
  _sampling_disabled = 1;
}

void
csprof_drop_sample(void)
{
  TMSG(DROP, "dropping sample");
  sigjmp_buf_t *it = &(TD_GET(bad_unwind));
  (*hpcrun_get_real_siglongjmp())(it->jb, 9);
}

long
csprof_num_samples_total(void)
{
  return num_samples_total;
}

// The async blocks happen in the signal handlers, without getting to
// hpcrun_sample_callpath, so also increment the total count here.
void
csprof_inc_samples_blocked_async(void)
{
  atomic_add_i64(&num_samples_total, 1L);
  atomic_add_i64(&num_samples_blocked_async, 1L);
}

void
csprof_inc_samples_filtered(void)
{
  atomic_add_i64(&num_samples_filtered, 1L);
}

void
csprof_display_summary(void)
{
  long blocked = num_samples_blocked_async + num_samples_blocked_dlopen;
  long errant = num_samples_dropped + num_samples_filtered;
  long other = num_samples_dropped - segv_count;
  long valid = num_samples_attempted - errant;

  hpcrun_memory_summary();

  AMSG("SAMPLE ANOMALIES: blocks: %ld (async: %ld, dlopen: %ld), "
       "errors: %ld (filtered: %ld, segv: %d, other: %ld)",
       blocked, num_samples_blocked_async, num_samples_blocked_dlopen,
       errant, num_samples_filtered, segv_count, other);

  AMSG("SUMMARY: samples: %ld (recorded: %ld, blocked: %ld, errant: %ld), "
       "intervals: %ld (suspicious: %ld)%s",
       num_samples_total, valid, blocked, errant,
       ui_count(), suspicious_count(),
       _sampling_disabled ? " SAMPLING WAS DISABLED" : "");
  // logs, retentions || adj.: recorded, retained, written

  if (ENABLED(UNW_VALID)) {
    hpcrun_validation_summary();
  }
}


cct_node_t *
hpcrun_sample_callpath(void *context, int metricId, uint64_t metricIncr,
		       int skipInner, int isSync)
{
  atomic_add_i64(&num_samples_total, 1L);

  if (_sampling_disabled){
    TMSG(SAMPLE,"global suspension");
    csprof_all_sources_stop();
    return NULL;
  }

  // Synchronous unwinds (pthread_create) must wait until they aquire
  // the read lock, but async samples give up if not avail.
  // This only applies in the dynamic case.
#ifndef HPCRUN_STATIC_LINK
  if (isSync) {
    while (! csprof_dlopen_read_lock()) ;
  }
  else if (! csprof_dlopen_read_lock()) {
    TMSG(SAMPLE, "skipping sample for dlopen lock");
    atomic_add_i64(&num_samples_blocked_dlopen, 1L);
    return NULL;
  }
#endif

  TMSG(SAMPLE, "attempting sample");
  atomic_add_i64(&num_samples_attempted, 1L);

  thread_data_t *td = hpcrun_get_thread_data();
  sigjmp_buf_t *it = &(td->bad_unwind);
  cct_node_t* node = NULL;
  state_t *state = td->state;

  csprof_set_handling_sample(td);

  int ljmp = sigsetjmp(it->jb, 1);
  if (ljmp == 0) {

    if (state != NULL) {
      node = _hpcrun_sample_callpath(state, context, metricId, metricIncr,
				     skipInner, isSync);

      if (trace_isactive()) {
	void *pc = context_pc(context);
	hpcrun_cct_t *cct = &(td->state->csdata); 
	void *func_start_pc, *func_end_pc;

	fnbounds_enclosing_addr(pc, &func_start_pc, &func_end_pc); 

	frame_t frm = {.ip = func_start_pc};
	cct_node_t* func_proxy = hpcrun_cct_get_child(cct, node->parent, &frm);
	func_proxy->persistent_id |= HPCRUN_FMT_RetainIdFlag; 

	trace_append(func_proxy->persistent_id);
      }
    }
  }
  else {
    // ------------------------------------------------------------
    // recover from SEGVs and dropped samples
    // ------------------------------------------------------------
    memset((void *)it->jb, '\0', sizeof(it->jb));
    dump_backtrace(state, td->unwind);
    atomic_add_i64(&num_samples_dropped, 1L);
    csprof_up_pmsg_count();
    if (TD_GET(splay_lock)) {
      csprof_release_splay_lock();
    }
    if (TD_GET(fnbounds_lock)) {
      fnbounds_release_lock();
    }
  }

  csprof_clear_handling_sample(td);
  if (TD_GET(mem_low) || ENABLED(FLUSH_EVERY_SAMPLE)) {
    hpcrun_finalize_current_epoch();
    hpcrun_flush_epochs();
    hpcrun_reclaim_freeable_mem();
  }
#ifndef HPCRUN_STATIC_LINK
  csprof_dlopen_read_unlock();
#endif
  
  return node;
}


static cct_node_t*
_hpcrun_sample_callpath(state_t *state, void *context,
			int metricId, uint64_t metricIncr, 
			int skipInner, int isSync)
{
  void *pc = context_pc(context);

  TMSG(SAMPLE,"csprof take profile sample @ %p",pc);

  /* check to see if shared library state has changed out from under us */
  state = hpcrun_check_for_new_epoch(state);

  cct_node_t* n =
    hpcrun_backtrace(state, context, metricId, metricIncr, skipInner, isSync);

  // FIXME: n == -1 if sample is filtered

  return n;
}
