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
// Copyright ((c)) 2002-2022, Rice University
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
#include "unwind.h"
#include <utilities/arch/context-pc.h>
#include "hpcrun-malloc.h"
#include "sample_event.h"
#include "sample_sources_all.h"
#include "start-stop.h"
#include "uw_recipe_map.h"
#include "validate_return_addr.h"
#include "write_data.h"
#include "cct_insert_backtrace.h"
#include "utilities/arch/context-pc.h"

#include <monitor.h>

#include <messages/messages.h>

#include <lib/prof-lean/hpcrun-fmt.h>

#define HPCRUN_DEBUG_TRACING 0

//*************************** Forward Declarations **************************


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

  if (TD_GET(fnbounds_lock)) {
    fnbounds_release_lock();
  }
}


static cct_node_t*
record_partial_unwind(
  cct_bundle_t* cct, frame_t* bt_beg,
  frame_t* bt_last, int metricId,
  hpcrun_metricVal_t metricIncr,
  int skipInner, void *data)
{
  if (ENABLED(NO_PARTIAL_UNW)){
    return NULL;
  }

  if (bt_last < bt_beg)
    bt_last = bt_beg;
  
  bt_beg = hpcrun_skip_chords(bt_last, bt_beg, skipInner);

  backtrace_info_t bt;

  bt.begin = bt_beg;
  bt.last =  bt_last;
  bt.fence = FENCE_BAD;
  bt.has_tramp = false;
  bt.n_trolls = 0;
  bt.bottom_frame_elided = false;

  TMSG(PARTIAL_UNW, "recording partial unwind from segv");
  hpcrun_stats_num_samples_partial_inc();
  return hpcrun_cct_record_backtrace_w_metric(cct, true, &bt,
//					      false, bt_beg, bt_last,
					      false, metricId, metricIncr, data);
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
		       hpcrun_metricVal_t metricIncr,
		       int skipInner, int isSync, sampling_info_t *data)
{
  uint64_t sampling_period = 0;
  int is_time_based_metric = 0;
  if (data != NULL) {
    sampling_period = data->sampling_period;
    is_time_based_metric = data->is_time_based_metric;
  }

  sample_val_t ret;
  hpcrun_sample_val_init(&ret);

  // if monitor_block_shootdown() returns a non-zero value
  // a thread is waiting to exit. if so, we can skip 
  // recording an asynchronous sample; however, synchronous
  // unwinds can't be skipped because the caller is
  // expecting a call path.
  int ready_to_exit = monitor_block_shootdown();
  if (!isSync && ready_to_exit) {
    monitor_unblock_shootdown();
    return ret;
  }

  // Sampling turned off by the user application.
  // This doesn't count as a sample for the summary stats.
  if (!isSync && !hpctoolkit_sampling_is_active()) {
    monitor_unblock_shootdown();
    return ret;
  }

  hpcrun_stats_num_samples_total_inc();

  if (!isSync && hpcrun_is_sampling_disabled()) {
    TMSG(SAMPLE,"global suspension");
    hpcrun_all_sources_stop();
    monitor_unblock_shootdown();
    return ret;
  }

  TMSG(SAMPLE_CALLPATH, "attempting sample");
  hpcrun_stats_num_samples_attempted_inc();

  thread_data_t* td   = hpcrun_get_thread_data();
  sigjmp_buf_t* it    = &(td->bad_unwind);
  sigjmp_buf_t* old   = td->current_jmp_buf;
  td->current_jmp_buf = it;

  cct_node_t* node = NULL;
  epoch_t* epoch = td->core_profile_trace_data.epoch;

  // --------------------------------------
  // start of handling sample
  // --------------------------------------
  hpcrun_set_handling_sample(td);

  td->btbuf_cur = NULL;
  td->deadlock_drop = false;
  int ljmp = sigsetjmp(it->jb, 1);
  if (ljmp == 0) {
    if (epoch != NULL) {
      void* pc = hpcrun_context_pc(context);

      TMSG(SAMPLE_CALLPATH, "%s taking profile sample @ %p", __func__, pc);
      TMSG(SAMPLE_METRIC_DATA, "--metric data for sample (as a uint64_t) = %"PRIu64"", metricIncr);

      /* check to see if shared library loadmap (of current epoch) has changed out from under us */
      epoch = hpcrun_check_for_new_loadmap(epoch);

      void *data_aux = NULL;
      if (data != NULL)
        data_aux = data->sample_data;

      node  = hpcrun_backtrace2cct(&(epoch->csdata), context, metricId,
                                   metricIncr, skipInner, isSync, data_aux);

      if (ENABLED(DUMP_BACKTRACES)) {
        hpcrun_bt_dump(td->btbuf_cur, "UNWIND");
      }
    }
  }
  else {  // Partial unwind case
    cct_bundle_t* cct = &(td->core_profile_trace_data.epoch->csdata);
    node = record_partial_unwind(cct, td->btbuf_beg, td->btbuf_cur - 1,
        metricId, metricIncr, skipInner, NULL);
    hpcrun_cleanup_partial_unwind();
  }
  td->current_jmp_buf = old;

  // --------------------------------------
  // end of handling sample
  // --------------------------------------

  ret.sample_node = node;

  cct_addr_t *addr = hpcrun_cct_addr(node);
  ip_normalized_t leaf_ip = addr->ip_norm;

  if (ip_normalized_eq(&leaf_ip, &(td->btbuf_beg->ip_norm))) {
    // the call chain sampled has as its leaf an instruction in a user
    // procedure. we know this because leaf_ip matches the first entry
    // in the backtrace buffer.  samples in kernel space yield a
    // leaf_ip that is not logged in the backtrace buffer. for user
    // space samples, the first entry in the backtrace buffer includes
    // not only the normalized IP of the call chain leaf but also the
    // IP of the first instruction in the enclosing function, which we
    // use to uniquely represent the function itself. in this case, we
    // adjust leaf_ip to point to the first IP of its enclosing
    // function to simplify processing of procedure-level traces for
    // call chains that are completely in user space.

    // when call chain tracing is enabled, tracing arbitrary leaf IPs
    // for user space call chains is messy because it can cause
    // trace-ids to be marked on multiple call chain leaves
    // (instructions) that belong to the same source-level
    // statement. collapsing these when call path traces are present
    // leaves us with many trace-ids referring to the same source
    // construct. trust me: merging here is easier :-).
    leaf_ip = td->btbuf_beg->the_function;
  }

  bool trace_ok = ! td->deadlock_drop;
  TMSG(TRACE1, "trace ok (!deadlock drop) = %d", trace_ok);
  if (trace_ok && hpcrun_trace_isactive() && !isSync && is_time_based_metric > 0) {
    TMSG(TRACE, "Sample event encountered");

    cct_addr_t frm;
    memset(&frm, 0, sizeof(cct_addr_t));
    frm.ip_norm = leaf_ip;

    TMSG(TRACE,"parent node = %p, &frm = %p", hpcrun_cct_parent(node), &frm);
    cct_node_t* func_proxy =
      hpcrun_cct_insert_addr(hpcrun_cct_parent(node), &frm);

    ret.trace_node = func_proxy;

    TMSG(TRACE, "Changed persistent id to indicate mutation of func_proxy node");
    hpcrun_trace_append(&td->core_profile_trace_data, func_proxy, metricId, td->prev_dLCA, sampling_period);
    TMSG(TRACE, "Appended func_proxy node to trace");
  }

  hpcrun_clear_handling_sample(td);
  if (get_mem_low() || ENABLED(FLUSH_EVERY_SAMPLE)) {
    hpcrun_flush_epochs(&(TD_GET(core_profile_trace_data)));
    hpcrun_reclaim_freeable_mem();
  }

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


  thread_data_t* td   = hpcrun_get_thread_data();
  sigjmp_buf_t* it    = &(td->bad_unwind);
  sigjmp_buf_t* old   = td->current_jmp_buf;
  td->current_jmp_buf = it;

  cct_node_t* node  = NULL;
  epoch_t* epoch    = td->core_profile_trace_data.epoch;

  hpcrun_set_handling_sample(td);

  td->btbuf_cur = NULL;
  int ljmp = sigsetjmp(it->jb, 1);
  backtrace_info_t bt;

  // initialize bt
  memset(&bt, 0, sizeof(bt));

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
        bt.has_tramp);
  }
  // restore back the sigjmp
  td->current_jmp_buf = old;

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
  if (get_mem_low() || ENABLED(FLUSH_EVERY_SAMPLE)) {
    hpcrun_flush_epochs(&(TD_GET(core_profile_trace_data)));
    hpcrun_reclaim_freeable_mem();
  }

  TMSG(THREAD,"done w pthread ctxt");
  monitor_unblock_shootdown();

  return node;
}

