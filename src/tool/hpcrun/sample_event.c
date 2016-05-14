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
#include "cct2metrics.h"
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
#include "start-stop.h"
#include "ui_tree.h"
#include "validate_return_addr.h"
#include "write_data.h"
#include "cct_insert_backtrace.h"

#include <monitor.h>

#include <messages/messages.h>

#include <lib/prof-lean/atomic-op.h>
#include <lib/prof-lean/hpcrun-fmt.h>

#define HPCRUN_DEBUG_TRACING 0

//*************************** Forward Declarations **************************

static cct_node_t*
help_hpcrun_sample_callpath(epoch_t *epoch, void *context, void **trace_pc,
			    int metricId, uint64_t metricIncr,
			    int skipInner, int isSync);

static cct_node_t*
hpcrun_dbg_sample_callpath(epoch_t *epoch, void *context, void **trace_pc,
			   int metricId, uint64_t metricIncr,
			   int skipInner, int isSync);

//***************************************************************************

//************************* Local helper routines ***************************

// ------------------------------------------------------------
// recover from SEGVs and partial unwinds
// ------------------------------------------------------------

static void
hpcrun_cleanup_partial_unwind(void)
{
  thread_data_t* td = hpcrun_get_thread_data();
  sigjmp_buf_t* it = &(td->bad_unwind);

  memset((void *)it->jb, '\0', sizeof(it->jb));

  if ( ! td->deadlock_drop)
    hpcrun_stats_num_samples_dropped_inc();

  hpcrun_up_pmsg_count();
  if (TD_GET(splay_lock)) {
    hpcrun_release_splay_lock();
  }
  if (TD_GET(fnbounds_lock)) {
    fnbounds_release_lock();
  }
}


static cct_node_t*
record_partial_unwind(
  cct_bundle_t* cct,
  frame_t* bt_beg, 
  frame_t* bt_last,
  int metricId, 
  uint64_t metricIncr,
  int skipInner)
{
  if (ENABLED(NO_PARTIAL_UNW)){
    return NULL;
  }

  bt_beg = hpcrun_skip_chords(bt_last, bt_beg, skipInner);

  backtrace_info_t bt;
 
  bt.begin = bt_beg;
  bt.last =  bt_last;
  bt.fence = FENCE_BAD;
  bt.has_tramp = false;
  bt.trolled = false;
  bt.n_trolls = 0;
  bt.bottom_frame_elided = false;

  TMSG(PARTIAL_UNW, "recording partial unwind from segv");
  hpcrun_stats_num_samples_partial_inc();
  return hpcrun_cct_record_backtrace_w_metric(cct, true, &bt,
//					      false, bt_beg, bt_last, 
					      false, metricId, metricIncr);
}



//***************************************************************************

bool private_hpcrun_sampling_disabled = false;

void
hpcrun_drop_sample(void)
{
  TMSG(DROP, "dropping sample");
  sigjmp_buf_t *it = &(TD_GET(bad_unwind));
  (*hpcrun_get_real_siglongjmp())(it->jb, 9);
}


sample_val_t
hpcrun_sample_callpath(void* context, int metricId,
		       uint64_t metricIncr,
		       int skipInner, int isSync)
{
  sample_val_t ret;
  hpcrun_sample_val_init(&ret);

  if (monitor_block_shootdown()) {
    monitor_unblock_shootdown();
    return ret;
  }

  // Sampling turned off by the user application.
  // This doesn't count as a sample for the summary stats.
  if (! hpctoolkit_sampling_is_active()) {
    return ret;
  }

  hpcrun_stats_num_samples_total_inc();

  if (hpcrun_is_sampling_disabled()) {
    TMSG(SAMPLE,"global suspension");
    hpcrun_all_sources_stop();
    monitor_unblock_shootdown();
    return ret;
  }

  // Synchronous unwinds (pthread_create) must wait until they acquire
  // the read lock, but async samples give up if not avail.
  // This only applies in the dynamic case.
#ifndef HPCRUN_STATIC_LINK
  if (isSync) {
    while (! hpcrun_dlopen_read_lock()) ;
  }
  else if (! hpcrun_dlopen_read_lock()) {
    TMSG(SAMPLE_CALLPATH, "skipping sample for dlopen lock");
    hpcrun_stats_num_samples_blocked_dlopen_inc();
    monitor_unblock_shootdown();
    return ret;
  }
#endif

  TMSG(SAMPLE_CALLPATH, "attempting sample");
  hpcrun_stats_num_samples_attempted_inc();

  thread_data_t* td = hpcrun_get_thread_data();
  sigjmp_buf_t* it = &(td->bad_unwind);
  cct_node_t* node = NULL;
  epoch_t* epoch = td->core_profile_trace_data.epoch;

  hpcrun_set_handling_sample(td);

  void* trace_pc = hpcrun_context_pc(context);

  td->btbuf_cur = NULL;
  td->deadlock_drop = false;
  int ljmp = sigsetjmp(it->jb, 1);
  if (ljmp == 0) {

    if (epoch != NULL) {
      if (ENABLED(DEBUG_PARTIAL_UNW)){
	EMSG("PARTIAL UNW debug sampler invoked @ sample %d", hpcrun_stats_num_samples_attempted());
	node = hpcrun_dbg_sample_callpath(epoch, context, &trace_pc, metricId, metricIncr,
					  skipInner, isSync);
      }
      else {
	node = help_hpcrun_sample_callpath(epoch, context, &trace_pc, metricId, metricIncr,
					   skipInner, isSync);
      }
      if (ENABLED(DUMP_BACKTRACES)) {
	hpcrun_bt_dump(td->btbuf_cur, "UNWIND");
      }
    }
  }
  else {
    cct_bundle_t* cct = &(td->core_profile_trace_data.epoch->csdata);
    node = record_partial_unwind(cct, td->btbuf_beg, td->btbuf_cur - 1,
				 metricId, metricIncr, skipInner);
    hpcrun_cleanup_partial_unwind();
  }

  ret.sample_node = node;

  bool trace_ok = ! td->deadlock_drop;
  TMSG(TRACE1, "trace ok (!deadlock drop) = %d", trace_ok);
  if (trace_ok && hpcrun_trace_isactive()) {

    void* func_start_pc = NULL;
    void* func_end_pc = NULL;
    load_module_t* lm = NULL;
    fnbounds_enclosing_addr(trace_pc, &func_start_pc, &func_end_pc, &lm);

    ip_normalized_t pc_proxy = hpcrun_normalize_ip(func_start_pc, lm);

    cct_addr_t frm = { .ip_norm = pc_proxy };
    cct_node_t* parent = hpcrun_cct_parent(node); 
    cct_node_t* func_proxy = hpcrun_cct_insert_addr(parent, &frm);

    ret.trace_node = func_proxy;

    // modify the persistent id
    hpcrun_cct_persistent_id_trace_mutate(func_proxy);
    hpcrun_trace_append(&td->core_profile_trace_data, hpcrun_cct_persistent_id(func_proxy), metricId);

#if HPCRUN_DEBUG_TRACING
    TMSG(TRACE, "pc = %p func start = %p, func_end = %p "
                "pc_proxy=(lm=%d,ip=%p) proxy pid=%d "
                "parent node %p (lm=%d,ip=%p) pid=%d",
                trace_pc,  func_start_pc, func_end_pc, 
                pc_proxy.lm_id, pc_proxy.lm_ip, hpcrun_cct_persistent_id(func_proxy),
                parent, 
                hpcrun_cct_addr(parent)->ip_norm.lm_id, hpcrun_cct_addr(parent)->ip_norm.lm_ip,
                hpcrun_cct_persistent_id(parent));
    {
      int i;
      cct_node_t* ancestor = parent;
      for(i = 0; ancestor; i++) { 
	      cct_addr_t *addr = hpcrun_cct_addr(ancestor);
	      TMSG(TRACE, "pc = %p "
                   "pc_proxy=(lm=%d,ip=%p) proxy pid=%d "
		   "ancestor %d node %p (lm=%d,ip=%p) pid=%d",
		   trace_pc, 
		   pc_proxy.lm_id, pc_proxy.lm_ip, hpcrun_cct_persistent_id(func_proxy),
		   i, ancestor, 
		   addr->ip_norm.lm_id, addr->ip_norm.lm_ip,
		   hpcrun_cct_persistent_id(ancestor));
	      ancestor = hpcrun_cct_parent(ancestor); 
      }
    }
#endif
  }

  hpcrun_clear_handling_sample(td);
  if (TD_GET(mem_low) || ENABLED(FLUSH_EVERY_SAMPLE)) {
    hpcrun_flush_epochs(&(TD_GET(core_profile_trace_data)));
    hpcrun_reclaim_freeable_mem();
  }
#ifndef HPCRUN_STATIC_LINK
  hpcrun_dlopen_read_unlock();
#endif
  
  TMSG(SAMPLE_CALLPATH,"done w sample, return %p", ret.sample_node);
  monitor_unblock_shootdown();

  return ret;
}

static int const PTHREAD_CTXT_SKIP_INNER = 1;

cct_node_t*
hpcrun_gen_thread_ctxt(void* context)
{
  if (monitor_block_shootdown()) {
    monitor_unblock_shootdown();
    return NULL;
  }

  if (hpcrun_is_sampling_disabled()) {
    TMSG(THREAD_CTXT,"global suspension");
    hpcrun_all_sources_stop();
    monitor_unblock_shootdown();
    return NULL;
  }

  // Synchronous unwinds (pthread_create) must wait until they acquire
  // the read lock, but async samples give up if not avail.
  // This only applies in the dynamic case.
#ifndef HPCRUN_STATIC_LINK
  while (! hpcrun_dlopen_read_lock()) ;
#endif

  thread_data_t* td = hpcrun_get_thread_data();
  sigjmp_buf_t* it = &(td->bad_unwind);
  cct_node_t* node = NULL;
  epoch_t* epoch = td->core_profile_trace_data.epoch;

  hpcrun_set_handling_sample(td);

  td->btbuf_cur = NULL;
  int ljmp = sigsetjmp(it->jb, 1);
  backtrace_info_t bt;
  if (ljmp == 0) {
    if (epoch != NULL) {
      if (! hpcrun_generate_backtrace_no_trampoline(&bt, context,
						    PTHREAD_CTXT_SKIP_INNER)) {
	hpcrun_clear_handling_sample(td); // restore state
	EMSG("Internal error: unable to obtain backtrace for pthread context");
	return NULL;
      }
    }
    //
    // If this backtrace is generated from sampling in a thread,
    // take off the top 'monitor_pthread_main' node
    //
    if ((epoch->csdata).ctxt && ! bt.has_tramp && (bt.fence == FENCE_THREAD)) {
      TMSG(THREAD_CTXT, "Thread correction, back off outermost backtrace entry");
      bt.last--;
    }
    node = hpcrun_cct_record_backtrace(&(epoch->csdata), false, &bt,
//				       bt.fence == FENCE_THREAD, bt.begin, bt.last, 
				       bt.has_tramp);
  }
  // FIXME: What to do when thread context is partial ?
#if 0
  else {
    cct_bundle_t* cct = &(td->epoch->csdata);
    node = record_partial_unwind(cct, td->btbuf_beg, td->btbuf_cur - 1,
				 metricId, metricIncr);
    hpcrun_cleanup_partial_unwind();
  }
#endif
  hpcrun_clear_handling_sample(td);
  if (TD_GET(mem_low) || ENABLED(FLUSH_EVERY_SAMPLE)) {
    hpcrun_flush_epochs(&(TD_GET(core_profile_trace_data)));
    hpcrun_reclaim_freeable_mem();
  }
#ifndef HPCRUN_STATIC_LINK
  hpcrun_dlopen_read_unlock();
#endif
  
  TMSG(THREAD,"done w pthread ctxt");
  monitor_unblock_shootdown();

  return node;
}

static cct_node_t*
hpcrun_dbg_sample_callpath(epoch_t *epoch, void *context, void **trace_pc,
			   int metricId,
			   uint64_t metricIncr, 
			   int skipInner, int isSync)
{
  void* pc = hpcrun_context_pc(context);

  TMSG(DEBUG_PARTIAL_UNW, "hpcrun take profile sample @ %p",pc);

  /* check to see if shared library loadmap (of current epoch) has changed out from under us */
  epoch = hpcrun_check_for_new_loadmap(epoch);

  cct_node_t* n = hpcrun_dbg_backtrace2cct(&(epoch->csdata),
					   context, trace_pc,
					   metricId, metricIncr,
					   skipInner);

  return n;
}


static cct_node_t*
help_hpcrun_sample_callpath(epoch_t *epoch, void *context, void **trace_pc,
			    int metricId,
			    uint64_t metricIncr, 
			    int skipInner, int isSync)
{
  void* pc = hpcrun_context_pc(context);

  TMSG(SAMPLE_CALLPATH, "%s taking profile sample @ %p", __func__, pc);
  TMSG(SAMPLE_METRIC_DATA, "--metric data for sample (as a uint64_t) = %"PRIu64"", metricIncr);

  /* check to see if shared library loadmap (of current epoch) has changed out from under us */
  epoch = hpcrun_check_for_new_loadmap(epoch);

  cct_node_t* n =
    hpcrun_backtrace2cct(&(epoch->csdata), context, trace_pc, metricId, metricIncr,
			 skipInner, isSync);

  // FIXME: n == -1 if sample is filtered

  return n;
}


#if 0 // TODO: tallent: Use Mike's improved code; retire prior routines
static cct_node_t*
help_hpcrun_sample_callpath_w_bt(epoch_t *epoch, void *context,
				 int metricId, uint64_t metricIncr,
				 bt_mut_fn bt_fn, bt_fn_arg arg,
				 int isSync);

cct_node_t*
hpcrun_sample_callpath_w_bt(void *context,
			    int metricId,
			    uint64_t metricIncr, 
			    bt_mut_fn bt_fn, bt_fn_arg arg,
			    int isSync)
{
  if (monitor_block_shootdown()) {
    monitor_unblock_shootdown();
    return NULL;
  }

  hpcrun_stats_num_samples_total_inc();

  if (hpcrun_is_sampling_disabled()) {
    TMSG(SAMPLE,"global suspension");
    hpcrun_all_sources_stop();
    monitor_unblock_shootdown();
    return NULL;
  }

  // Synchronous unwinds (pthread_create) must wait until they acquire
  // the read lock, but async samples give up if not avail.
  // This only applies in the dynamic case.
#ifndef HPCRUN_STATIC_LINK
  if (isSync) {
    while (! hpcrun_dlopen_read_lock()) ;
  }
  else if (! hpcrun_dlopen_read_lock()) {
    TMSG(SAMPLE, "skipping sample for dlopen lock");
    hpcrun_stats_num_samples_blocked_dlopen_inc();
    monitor_unblock_shootdown();
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
      node = help_hpcrun_sample_callpath_w_bt(epoch, context, metricId, metricIncr,
					      bt_fn, arg, isSync);

      if (hpcrun_trace_isactive()) {
	TMSG(TRACE, "non-NULL epoch code");
	void* pc = hpcrun_context_pc(context);
	cct_bundle_t* cct = &(td->epoch->csdata); 
	void* func_start_pc = NULL;
	void* func_end_pc   = NULL;

	fnbounds_enclosing_addr(pc, &func_start_pc, &func_end_pc); 

	frame_t frm = {.ip = func_start_pc};
	cct_node_t* func_proxy = hpcrun_cct_get_child(cct, node->parent, &frm);
	func_proxy->persistent_id |= HPCRUN_FMT_RetainIdFlag; 

	hpcrun_trace_append(func_proxy->persistent_id);
      }
      if (ENABLED(DUMP_BACKTRACES)) {
	hpcrun_bt_dump(td->btbuf_cur, "UNWIND");
      }
    }
  }
  else {
    hpcrun_cleanup_partial_unwind();
  }

  hpcrun_clear_handling_sample(td);
  if (TD_GET(mem_low) || ENABLED(FLUSH_EVERY_SAMPLE)) {
    hpcrun_finalize_current_loadmap();
    hpcrun_flush_epochs(&(TD_GET(core_profile_trace_data)));
    hpcrun_reclaim_freeable_mem();
  }
#ifndef HPCRUN_STATIC_LINK
  hpcrun_dlopen_read_unlock();
#endif

  monitor_unblock_shootdown();
  
  return node;
}


static cct_node_t*
help_hpcrun_sample_callpath_w_bt(epoch_t* epoch, void *context,
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

#endif
