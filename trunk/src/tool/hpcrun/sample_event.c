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


#include <setjmp.h>
#include <string.h>

//*************************** User Include Files ****************************

#include <unwind/common/backtrace.h>
#include <cct/cct.h>
#include "hpcrun_dlfns.h"
#include "hpcrun_stats.h"
#include "hpcrun-malloc.h"
#include "fnbounds_interface.h"
#include "main.h"
#include "metrics_types.h"
#include "metrics.h"
#include "segv_handler.h"
#include "epoch.h"
#include "thread_data.h"
#include "trace.h"
#include "handling_sample.h"
#include "interval-interface.h"
#include "unwind.h"
#include <utilities/arch/context-pc.h>
#include "hpcrun-malloc.h"
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
_hpcrun_sample_callpath(epoch_t *epoch, void *context,
			int metricId, uint64_t metricIncr,
			int skipInner, int isSync);

static cct_node_t*
_hpcrun_sample_callpath_w_bt(epoch_t *epoch, void *context,
			     int metricId, uint64_t metricIncr,
			     bt_mut_fn bt_fn, bt_fn_arg arg,
			     int isSync);

// ------------------------------------------------------------
// recover from SEGVs and dropped samples
// ------------------------------------------------------------

static void
_drop_sample(void)
{
  thread_data_t* td = hpcrun_get_thread_data();
  sigjmp_buf_t* it = &(td->bad_unwind);

  memset((void *)it->jb, '\0', sizeof(it->jb));
  hpcrun_bt_dump(td->unwind, "SEGV");

  hpcrun_stats_num_samples_dropped_inc();

  hpcrun_up_pmsg_count();
  if (TD_GET(splay_lock)) {
    hpcrun_release_splay_lock();
  }
  if (TD_GET(fnbounds_lock)) {
    fnbounds_release_lock();
  }
}

//***************************************************************************

bool _hpcrun_sampling_disabled = false;

void
hpcrun_disable_sampling(void)
{
  TMSG(SPECIAL,"Sampling disabled");
  _hpcrun_sampling_disabled = true;
}

void
hpcrun_drop_sample(void)
{
  TMSG(DROP, "dropping sample");
  sigjmp_buf_t *it = &(TD_GET(bad_unwind));
  (*hpcrun_get_real_siglongjmp())(it->jb, 9);
}


cct_node_t*
hpcrun_sample_callpath(void *context, int metricId,
		       uint64_t metricIncr,
		       int skipInner, int isSync)
{
  hpcrun_stats_num_samples_total_inc();

  if (hpcrun_is_sampling_disabled()) {
    TMSG(SAMPLE,"global suspension");
    hpcrun_all_sources_stop();
    return NULL;
  }

  // Synchronous unwinds (pthread_create) must wait until they aquire
  // the read lock, but async samples give up if not avail.
  // This only applies in the dynamic case.
#ifndef HPCRUN_STATIC_LINK
  if (isSync) {
    while (! hpcrun_dlopen_read_lock()) ;
  }
  else if (! hpcrun_dlopen_read_lock()) {
    TMSG(SAMPLE, "skipping sample for dlopen lock");
    hpcrun_stats_num_samples_blocked_dlopen_inc();
    return NULL;
  }
#endif

  TMSG(SAMPLE, "attempting sample");
  hpcrun_stats_num_samples_attempted_inc();

  thread_data_t* td = hpcrun_get_thread_data();
  sigjmp_buf_t* it = &(td->bad_unwind);
  cct_node_t* node = NULL;
  epoch_t* epoch = td->epoch;

  hpcrun_set_handling_sample(td);

  td->unwind = NULL;
  int ljmp = sigsetjmp(it->jb, 1);
  if (ljmp == 0) {

    if (epoch != NULL) {
      node = _hpcrun_sample_callpath(epoch, context, metricId, metricIncr,
				     skipInner, isSync);

      if (trace_isactive()) {
	void *pc = hpcrun_context_pc(context);
	hpcrun_cct_t *cct = &(td->epoch->csdata); 
	void *func_start_pc, *func_end_pc;

	fnbounds_enclosing_addr(pc, &func_start_pc, &func_end_pc); 

	frame_t frm = {.ip = func_start_pc};
	cct_node_t* func_proxy = hpcrun_cct_get_child(cct, node->parent, &frm);
	func_proxy->persistent_id |= HPCRUN_FMT_RetainIdFlag; 

	trace_append(func_proxy->persistent_id);
      }
      if (ENABLED(DUMP_BACKTRACES)) {
	hpcrun_bt_dump(td->unwind, "UNWIND");
      }
    }
  }
  else {
    _drop_sample();
  }

  hpcrun_clear_handling_sample(td);
  if (TD_GET(mem_low) || ENABLED(FLUSH_EVERY_SAMPLE)) {
    hpcrun_finalize_current_loadmap();
    hpcrun_flush_epochs();
    hpcrun_reclaim_freeable_mem();
  }
#ifndef HPCRUN_STATIC_LINK
  hpcrun_dlopen_read_unlock();
#endif
  
  return node;
}


static cct_node_t*
_hpcrun_sample_callpath_w_bt(epoch_t* epoch, void *context,
			     int metricId, uint64_t metricIncr,
			     bt_mut_fn bt_fn, bt_fn_arg arg,
			     int isSync)
{
  void* pc = hpcrun_context_pc(context);

  TMSG(SAMPLE,"csprof take profile sample @ %p",pc);

  /* check to see if shared library loadmap (of current epoch) has changed out from under us */
  epoch = hpcrun_check_for_new_loadmap(epoch);

  cct_node_t* n =
    hpcrun_bt2cct(&(epoch->csdata), context, metricId, metricIncr, bt_fn, arg, isSync);

  // FIXME: n == -1 if sample is filtered

  return n;
}

static cct_node_t*
_hpcrun_sample_callpath(epoch_t *epoch, void *context,
			int metricId,
			uint64_t metricIncr, 
			int skipInner, int isSync)
{
  void* pc = hpcrun_context_pc(context);

  TMSG(SAMPLE,"csprof take profile sample @ %p",pc);

  /* check to see if shared library loadmap (of current epoch) has changed out from under us */
  epoch = hpcrun_check_for_new_loadmap(epoch);

  cct_node_t* n =
    hpcrun_backtrace2cct(&(epoch->csdata), context, metricId, metricIncr, skipInner, isSync);

  // FIXME: n == -1 if sample is filtered

  return n;
}

cct_node_t*
hpcrun_sample_callpath_w_bt(void *context,
			    int metricId,
			    uint64_t metricIncr, 
			    bt_mut_fn bt_fn, bt_fn_arg arg,
			    int isSync)

{
  hpcrun_stats_num_samples_total_inc();

  if (hpcrun_is_sampling_disabled()) {
    TMSG(SAMPLE,"global suspension");
    hpcrun_all_sources_stop();
    return NULL;
  }

  // Synchronous unwinds (pthread_create) must wait until they aquire
  // the read lock, but async samples give up if not avail.
  // This only applies in the dynamic case.
#ifndef HPCRUN_STATIC_LINK
  if (isSync) {
    while (! hpcrun_dlopen_read_lock()) ;
  }
  else if (! hpcrun_dlopen_read_lock()) {
    TMSG(SAMPLE, "skipping sample for dlopen lock");
    hpcrun_stats_num_samples_blocked_dlopen_inc();
    return NULL;
  }
#endif

  TMSG(SAMPLE, "attempting sample");
  hpcrun_stats_num_samples_attempted_inc();

  thread_data_t* td = hpcrun_get_thread_data();
  sigjmp_buf_t* it = &(td->bad_unwind);
  cct_node_t* node = NULL;
  epoch_t* epoch = td->epoch;

  hpcrun_set_handling_sample(td);

  int ljmp = sigsetjmp(it->jb, 1);
  if (ljmp == 0) {

    if (epoch != NULL) {
      node = _hpcrun_sample_callpath_w_bt(epoch, context, metricId, metricIncr,
					  bt_fn, arg, isSync);

      if (trace_isactive()) {
	void *pc = hpcrun_context_pc(context);
	hpcrun_cct_t *cct = &(td->epoch->csdata); 
	void *func_start_pc, *func_end_pc;

	fnbounds_enclosing_addr(pc, &func_start_pc, &func_end_pc); 

	frame_t frm = {.ip = func_start_pc};
	cct_node_t* func_proxy = hpcrun_cct_get_child(cct, node->parent, &frm);
	func_proxy->persistent_id |= HPCRUN_FMT_RetainIdFlag; 

	trace_append(func_proxy->persistent_id);
      }
      if (ENABLED(DUMP_BACKTRACES)) {
	hpcrun_bt_dump(td->unwind, "UNWIND");
      }
    }
  }
  else {
    // ------------------------------------------------------------
    // recover from SEGVs and dropped samples
    // ------------------------------------------------------------
    memset((void *)it->jb, '\0', sizeof(it->jb));
    hpcrun_bt_dump(td->unwind, "SEGV");

    hpcrun_stats_num_samples_dropped_inc();

    hpcrun_up_pmsg_count();
    if (TD_GET(splay_lock)) {
      hpcrun_release_splay_lock();
    }
    if (TD_GET(fnbounds_lock)) {
      fnbounds_release_lock();
    }
  }

  hpcrun_clear_handling_sample(td);
  if (TD_GET(mem_low) || ENABLED(FLUSH_EVERY_SAMPLE)) {
    hpcrun_finalize_current_loadmap();
    hpcrun_flush_epochs();
    hpcrun_reclaim_freeable_mem();
  }
#ifndef HPCRUN_STATIC_LINK
  hpcrun_dlopen_read_unlock();
#endif
  
  return node;
}
