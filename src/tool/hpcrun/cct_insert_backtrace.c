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
// Copyright ((c)) 2002-2013, Rice University
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

#include <stdint.h>
#include <stdbool.h>
#include <ucontext.h>

#include <cct/cct_bundle.h>
#include <cct/cct.h>
#include <messages/messages.h>
#include <hpcrun/metrics.h>
#include <lib/prof-lean/lush/lush-support.h>
#include <lush/lush-backtrace.h>
#include <thread_data.h>
#include <hpcrun_stats.h>
#include <trampoline/common/trampoline.h>
#include <utilities/ip-normalized.h>
#include "frame.h"
#include <unwind/common/backtrace_info.h>
#include <unwind/common/fence_enum.h>
#include "cct_insert_backtrace.h"

//
// Misc externals (not in an include file)
//
extern bool hpcrun_inbounds_main(void* addr);

//
// local variable records the on/off state of
// special recursive compression:
//
static bool retain_recursion = false;

static cct_node_t*
cct_insert_raw_backtrace(cct_node_t* cct,
                            frame_t* path_beg, frame_t* path_end)
{
  TMSG(BT_INSERT, "%s : start", __func__);
  if ( (path_beg < path_end) || (!cct)) {
    TMSG(BT_INSERT, "No insert effect, cct = %p, path_beg = %p, path_end = %p",
	 cct, path_beg, path_end);
    return cct;
  }
  ip_normalized_t parent_routine = ip_normalized_NULL;
  for(; path_beg >= path_end; path_beg--){
    if ( (! retain_recursion) &&
	 (path_beg >= path_end + 1) && 
         ip_normalized_eq(&(path_beg->the_function), &(parent_routine)) &&
	 ip_normalized_eq(&(path_beg->the_function), &((path_beg-1)->the_function))) { 
      TMSG(REC_COMPRESS, "recursive routine compression!");
    }
    else {
      cct_addr_t tmp = (cct_addr_t) {.as_info = path_beg->as_info, .ip_norm = path_beg->ip_norm, .lip = path_beg->lip};
      TMSG(BT_INSERT, "inserting addr (%d, %p)", tmp.ip_norm.lm_id, tmp.ip_norm.lm_ip);
      cct = hpcrun_cct_insert_addr(cct, &tmp);
    }
    parent_routine = path_beg->the_function;
  }
  hpcrun_cct_terminate_path(cct);
  return cct;
}

static cct_node_t*
help_hpcrun_backtrace2cct(cct_bundle_t* cct, ucontext_t* context,
			  int metricId, uint64_t metricIncr,
			  int skipInner, int isSync);

//
// 
void
hpcrun_set_retain_recursion_mode(bool mode)
{
  TMSG(REC_COMPRESS, "retain_recursion set to %s", mode ? "true" : "false");
  retain_recursion = mode;
}

// See usage in header.
cct_node_t*
hpcrun_cct_insert_backtrace(cct_node_t* treenode, frame_t* path_beg, frame_t* path_end)
{
  TMSG(FENCE, "insert backtrace into treenode %p", treenode);
  TMSG(FENCE, "backtrace below");
  bool bt_ins = ENABLED(BT_INSERT);
  if (ENABLED(FENCE)) {
    ENABLE(BT_INSERT);
  }

  cct_node_t* path = cct_insert_raw_backtrace(treenode, path_beg, path_end);
  if (! bt_ins) DISABLE(BT_INSERT);

  // Put lush as_info class correction here

  // N.B. If 'frm' is a 1-to-1 bichord and 'path' is not (i.e., 'path'
  // is M-to-1 or 1-to-M), then update the association of 'path' to
  // reflect that 'path' is now a proxy for two bichord types (1-to-1
  // and M-to-1 or 1-to-M)

  cct_addr_t* addr = hpcrun_cct_addr(path);

  lush_assoc_t as_frm = lush_assoc_info__get_assoc(path_beg->as_info);
  lush_assoc_t as_path = lush_assoc_info__get_assoc(addr->as_info);

  if (as_frm == LUSH_ASSOC_1_to_1 && as_path != LUSH_ASSOC_1_to_1) {
    // INVARIANT: path->as_info should be either M-to-1 or 1-to-M
    lush_assoc_info__set_assoc(hpcrun_cct_addr(path)->as_info, LUSH_ASSOC_1_to_1);
  }
  return path;
}

// See usage in header.
cct_node_t*
hpcrun_cct_insert_backtrace_w_metric(cct_node_t* treenode,
				     int metric_id,
				     frame_t* path_beg, frame_t* path_end,
				     cct_metric_data_t datum)
{
  cct_node_t* path = hpcrun_cct_insert_backtrace(treenode, path_beg, path_end);

  metric_set_t* mset = hpcrun_reify_metric_set(path);

  metric_upd_proc_t* upd_proc = hpcrun_get_metric_proc(metric_id);
  if (upd_proc) {
    upd_proc(metric_id, mset, datum);
  }

  // POST-INVARIANT: metric set has been allocated for 'path'

  return path;
}

//
// Insert new backtrace in cct
//

cct_node_t*
hpcrun_cct_insert_bt(cct_node_t* node,
		     int metricId,
		     backtrace_t* bt,
		     cct_metric_data_t datum)
{
  return hpcrun_cct_insert_backtrace_w_metric(node, metricId, hpcrun_bt_last(bt), hpcrun_bt_beg(bt), datum);
}


//-----------------------------------------------------------------------------
// function: hpcrun_backtrace2cct
// purpose:
//     if successful, returns the leaf node representing the sample;
//     otherwise, returns NULL.
//-----------------------------------------------------------------------------

//
// TODO: one more flag:
//   backtrace needs to either:
//       IGNORE_TRAMPOLINE (usually, but not always called when isSync is true)
//       PLACE_TRAMPOLINE  (standard for normal async samples).
//             

cct_node_t*
hpcrun_backtrace2cct(cct_bundle_t* cct, ucontext_t* context, 
		     int metricId,
		     uint64_t metricIncr,
		     int skipInner, int isSync)
{
  cct_node_t* n = NULL;
  if (hpcrun_isLogicalUnwind()) {
    TMSG(LUSH,"lush backtrace2cct invoked");
    n = lush_backtrace2cct(cct, context, metricId, metricIncr, skipInner,
			   isSync);
  }
  else {
    TMSG(BT_INSERT,"regular (NON-lush) backtrace2cct invoked");
    n = help_hpcrun_backtrace2cct(cct, context,
				  metricId, metricIncr,
				  skipInner, isSync);
  }

  // N.B.: for lush_backtrace() it may be that n = NULL

  return n;
}

#if 0 // TODO: tallent: Use Mike's improved code; retire prior routines

static cct_node_t*
help_hpcrun_bt2cct(cct_bundle_t *cct, ucontext_t* context,
	       int metricId, uint64_t metricIncr,
	       bt_mut_fn bt_fn, bt_fn_arg bt_arg);

//
// utility routine that does 3 things:
//   1) Generate a std backtrace
//   2) Modifies the backtrace according to a passed in function
//   3) enters the generated backtrace in the cct
//
cct_node_t*
hpcrun_bt2cct(cct_bundle_t *cct, ucontext_t* context,
	      int metricId, uint64_t metricIncr,
	      bt_mut_fn bt_fn, bt_fn_arg arg, int isSync)
{
  cct_node_t* n = NULL;
  if (hpcrun_isLogicalUnwind()) {
#ifdef LATER
    TMSG(LUSH,"lush backtrace2cct invoked");
    n = lush_backtrace2cct(cct, context, metricId, metricIncr, skipInner,
			   isSync);
#endif
  }
  else {
    TMSG(LUSH,"regular (NON-lush) bt2cct invoked");
    n = help_hpcrun_bt2cct(cct, context, metricId, metricIncr, bt_fn, arg);
  }

  // N.B.: for lush_backtrace() it may be that n = NULL

  return n;
}

#endif

void
monitored_heap_data(void)
{
}

void
monitored_static_data(void)
{
}

void
monitored_stack_data(void)
{
}

void
monitored_unknown_data(void)
{
}

void
NOT_data_access(void)
{
}

void
heap_data_accesses(void)
{
}

void
heap_data_allocation(void)
{
}

void
data_range_1_5()
{
}

void
data_range_2_5()
{
}

void
data_range_3_5()
{
}

void
data_range_4_5()
{
}

void
data_range_5_5()
{
}

cct_node_t*
hpcrun_cct_record_backtrace(cct_bundle_t* cct, bool partial, bool thread_stop,
			    frame_t* bt_beg, frame_t* bt_last, bool tramp_found)
{
  TMSG(FENCE, "Recording backtrace");
  thread_data_t* td = hpcrun_get_thread_data();
  cct_node_t* cct_cursor = cct->tree_root;
  TMSG(FENCE, "Initially picking tree root = %p", cct_cursor);
  if (tramp_found) {
    // start insertion below caller's frame, which is marked with the trampoline
    cct_cursor = hpcrun_cct_parent(td->tramp_cct_node);
    TMSG(FENCE, "Tramp found ==> cursor = %p", cct_cursor);
  }
  if (partial) {
    cct_cursor = cct->partial_unw_root;
    TMSG(FENCE, "Partial unwind ==> cursor = %p", cct_cursor);
  }
  if (thread_stop) {
    cct_cursor = cct->thread_root;
    TMSG(FENCE, "Thread stop ==> cursor = %p", cct_cursor);
  }

  // datacentric support: attach samples to data allocation cct
  cct_node_t *data_node = TD_GET(data_node);
  if(data_node) {
    cct_node_t *prefix = hpcrun_cct_insert_path_return_leaf(data_node, cct_cursor);
    cct_cursor = prefix;
    TD_GET(data_node) = NULL;
  }

  TMSG(FENCE, "sanity check cursor = %p", cct_cursor);
  TMSG(FENCE, "further sanity check: bt_last frame = (%d, %p)", bt_last->ip_norm.lm_id, bt_last->ip_norm.lm_ip);
  return hpcrun_cct_insert_backtrace(cct_cursor, bt_last, bt_beg);

}

cct_node_t*
hpcrun_cct_record_backtrace_w_metric(cct_bundle_t* cct, bool partial, bool thread_stop,
				     frame_t* bt_beg, frame_t* bt_last, bool tramp_found,
				     int metricId, uint64_t metricIncr)
{
  TMSG(FENCE, "Recording backtrace");
  TMSG(BT_INSERT, "Record backtrace w metric to id %d, incr = %d", metricId, metricIncr);
  thread_data_t* td = hpcrun_get_thread_data();
  cct_node_t* cct_cursor = cct->tree_root;
  TMSG(FENCE, "Initially picking tree root = %p", cct_cursor);
  if (tramp_found) {
    // start insertion below caller's frame, which is marked with the trampoline
    cct_cursor = hpcrun_cct_parent(td->tramp_cct_node);
    TMSG(FENCE, "Tramp found ==> cursor = %p", cct_cursor);
  }
  if (partial) {
    cct_cursor = cct->partial_unw_root;
    TMSG(FENCE, "Partial unwind ==> cursor = %p", cct_cursor);
  }
  if (thread_stop) {
    cct_cursor = cct->thread_root;
    TMSG(FENCE, "Thread stop ==> cursor = %p", cct_cursor);
  }

  // make sure at least two cct(s) are created (not delete the single root in cct normalization)
#if defined(__PPC64__) || defined(HOST_CPU_IA64)
  cct_cursor = hpcrun_insert_special_node(cct->tree_root, *(void **)NOT_data_access);
#else
  cct_cursor = hpcrun_insert_special_node(cct->tree_root, NOT_data_access);
#endif
  // datacentric support: attach samples to data allocation cct
  cct_node_t *data_node = TD_GET(data_node);
  uint16_t lm_id = TD_GET(lm_id);
  uintptr_t lm_ip = TD_GET(lm_ip);
  
  void *ea = TD_GET(ea);
  void *start = TD_GET(start);
  void *end = TD_GET(end);

  if(data_node) {
    // give a tag for the heap allocated data
#if defined(__PPC64__) || defined(HOST_CPU_IA64)
    cct_cursor = hpcrun_insert_special_node(cct->tree_root, *(void **)monitored_heap_data);
#else
    cct_cursor = hpcrun_insert_special_node(cct->tree_root, monitored_heap_data);
#endif

    // copy the call path of the malloc
    cct_cursor = hpcrun_cct_insert_path_return_leaf(data_node, cct_cursor);

    // insert address-centric markers
    if (ENABLED(ADDRESS_CENTRIC)) {
      // first compute the range the address falls in
      double ratio = (double)(ea-start)/(end-start);
      if (ratio < 0.2) {
#if defined(__PPC64__) || defined(HOST_CPU_IA64)
        cct_cursor = hpcrun_insert_special_node(cct_cursor, *(void **)data_range_1_5);
#else
        cct_cursor = hpcrun_insert_special_node(cct_cursor, data_range_1_5);
#endif
      }
      else if (ratio < 0.4 && ratio >= 0.2 ) {
#if defined(__PPC64__) || defined(HOST_CPU_IA64)
        cct_cursor = hpcrun_insert_special_node(cct_cursor, *(void **)data_range_2_5);
#else
        cct_cursor = hpcrun_insert_special_node(cct_cursor, data_range_2_5);
#endif
      }
      else if (ratio < 0.6 && ratio >= 0.4 ) {
#if defined(__PPC64__) || defined(HOST_CPU_IA64)
        cct_cursor = hpcrun_insert_special_node(cct_cursor, *(void **)data_range_3_5);
#else
        cct_cursor = hpcrun_insert_special_node(cct_cursor, data_range_3_5);
#endif
      }
      else if (ratio < 0.8 && ratio >= 0.6 ) {
#if defined(__PPC64__) || defined(HOST_CPU_IA64)
        cct_cursor = hpcrun_insert_special_node(cct_cursor, *(void **)data_range_4_5);
#else
        cct_cursor = hpcrun_insert_special_node(cct_cursor, data_range_4_5);
#endif
      }
      else {
#if defined(__PPC64__) || defined(HOST_CPU_IA64)
        cct_cursor = hpcrun_insert_special_node(cct_cursor, *(void **)data_range_5_5);
#else
        cct_cursor = hpcrun_insert_special_node(cct_cursor, data_range_5_5);
#endif
      }
    }

#if defined(__PPC64__) || defined(HOST_CPU_IA64)
    cct_cursor = hpcrun_insert_special_node(cct_cursor, *(void **)heap_data_accesses);
#else
    cct_cursor = hpcrun_insert_special_node(cct_cursor, heap_data_accesses);
#endif
    TD_GET(data_node) = NULL;
  }
  // static data
  else if(lm_ip){
#if defined(__PPC64__) || defined(HOST_CPU_IA64)
    cct_cursor = hpcrun_insert_special_node(cct->tree_root, *(void **)monitored_static_data);
#else
    cct_cursor = hpcrun_insert_special_node(cct->tree_root, monitored_static_data);
#endif
  
    cct_cursor = hpcrun_cct_insert_addr(cct_cursor, &ADDR2(lm_id, lm_ip+1));
    TD_GET(lm_id) = 0;
    TD_GET(lm_ip) = NULL;
    // insert address-centric markers
    if (ENABLED(ADDRESS_CENTRIC)) {
      // first compute the range the address falls in
      double ratio = (double)(ea-start)/(end-start);
      if (ratio < 0.2) {
#if defined(__PPC64__) || defined(HOST_CPU_IA64)
        cct_cursor = hpcrun_insert_special_node(cct_cursor, *(void **)data_range_1_5);
#else
        cct_cursor = hpcrun_insert_special_node(cct_cursor, data_range_1_5);
#endif
      }
      else if (ratio < 0.4 && ratio >= 0.2 ) {
#if defined(__PPC64__) || defined(HOST_CPU_IA64)
        cct_cursor = hpcrun_insert_special_node(cct_cursor, *(void **)data_range_2_5);
#else
        cct_cursor = hpcrun_insert_special_node(cct_cursor, data_range_2_5);
#endif
      }
      else if (ratio < 0.6 && ratio >= 0.4 ) {
#if defined(__PPC64__) || defined(HOST_CPU_IA64)
        cct_cursor = hpcrun_insert_special_node(cct_cursor, *(void **)data_range_3_5);
#else
        cct_cursor = hpcrun_insert_special_node(cct_cursor, data_range_3_5);
#endif
      }
      else if (ratio < 0.8 && ratio >= 0.6 ) {
#if defined(__PPC64__) || defined(HOST_CPU_IA64)
        cct_cursor = hpcrun_insert_special_node(cct_cursor, *(void **)data_range_4_5);
#else
        cct_cursor = hpcrun_insert_special_node(cct_cursor, data_range_4_5);
#endif
      }
      else {
#if defined(__PPC64__) || defined(HOST_CPU_IA64)
        cct_cursor = hpcrun_insert_special_node(cct_cursor, *(void **)data_range_5_5);
#else
        cct_cursor = hpcrun_insert_special_node(cct_cursor, data_range_5_5);
#endif
      }
    }
  }
  // stack or c++ template data, cannot back to data
  // or hardware does not give effective address back
  else if(TD_GET(ldst) == 1){
#if defined(__PPC64__) || defined(HOST_CPU_IA64)
    cct_cursor = hpcrun_insert_special_node(cct->tree_root, *(void **)monitored_unknown_data);
#else
    cct_cursor = hpcrun_insert_special_node(cct->tree_root, monitored_unknown_data);
#endif
  }
  // unwind for malloc
  else if (TD_GET(in_malloc) == 1) {
#if defined(__PPC64__) || defined(HOST_CPU_IA64)
    cct_cursor = hpcrun_insert_special_node(cct->tree_root, *(void **)heap_data_allocation);
#else
    cct_cursor = hpcrun_insert_special_node(cct->tree_root, heap_data_allocation);
#endif
  }
  // not datacentric related samples in the datacentric profiling
  else if(ENABLED(DATACENTRIC)){
#if defined(__PPC64__) || defined(HOST_CPU_IA64)
    cct_cursor = hpcrun_insert_special_node(cct->tree_root, *(void **)NOT_data_access);
#else
    cct_cursor = hpcrun_insert_special_node(cct->tree_root, NOT_data_access);
#endif
  }
  // regular profiling
  else {
  }

  TMSG(FENCE, "sanity check cursor = %p", cct_cursor);
  TMSG(FENCE, "further sanity check: bt_last frame = (%d, %p)", bt_last->ip_norm.lm_id, bt_last->ip_norm.lm_ip);
  return hpcrun_cct_insert_backtrace_w_metric(cct_cursor, metricId,
					      bt_last, bt_beg,
					      (cct_metric_data_t){.i = metricIncr});

}

cct_node_t*
hpcrun_dbg_backtrace2cct(cct_bundle_t* cct, ucontext_t* context,
			 int metricId,
			 uint64_t metricIncr,
			 int skipInner)
{
  thread_data_t* td = hpcrun_get_thread_data();
  backtrace_info_t bt;

  if (! hpcrun_dbg_generate_backtrace(&bt, context, skipInner)) {
    if (ENABLED(NO_PARTIAL_UNW)){
      return NULL;
    }
    else {
      TMSG(PARTIAL_UNW, "recording partial unwind from graceful failure");
      hpcrun_stats_num_samples_partial_inc();
    }
  }

  cct_node_t* n = hpcrun_cct_record_backtrace_w_metric(cct, true, bt.fence == FENCE_THREAD,
						       bt.begin, bt.last, bt.has_tramp,
						       metricId, metricIncr);

  hpcrun_stats_frames_total_inc((long)(bt.last - bt.begin + 1));
  hpcrun_stats_trolled_frames_inc((long) bt.n_trolls);

  if (ENABLED(USE_TRAMP)){
    hpcrun_trampoline_remove();
    td->tramp_frame = td->cached_bt;
    hpcrun_trampoline_insert(n);
  }

  return n;
}

static cct_node_t*
help_hpcrun_backtrace2cct(cct_bundle_t* cct, ucontext_t* context,
			  int metricId,
			  uint64_t metricIncr,
			  int skipInner, int isSync)
{
  bool tramp_found;

  thread_data_t* td = hpcrun_get_thread_data();
  backtrace_info_t bt;

  bool partial_unw = false;
  if (! hpcrun_generate_backtrace(&bt, context, skipInner)) {
    if (ENABLED(NO_PARTIAL_UNW)){
      return NULL;
    }

    TMSG(PARTIAL_UNW, "recording partial unwind from graceful failure, "
	 "len partial unw = %d", (bt.last - bt.begin)+1);
    hpcrun_stats_num_samples_partial_inc();
    partial_unw = true;
  }

  frame_t* bt_beg = bt.begin;
  frame_t* bt_last = bt.last;

  tramp_found = bt.has_tramp;

  //
  // Check to make sure node below monitor_main is "main" node
  //
  // TMSG(GENERIC1, "tmain chk");
  if ( bt.fence == FENCE_MAIN &&
       ! partial_unw &&
       ! tramp_found &&
       (bt_last == bt_beg || 
	! hpcrun_inbounds_main(hpcrun_frame_get_unnorm(bt_last - 1)))) {
    hpcrun_bt_dump(TD_GET(btbuf_cur), "WRONG MAIN");
    hpcrun_stats_num_samples_dropped_inc();
    partial_unw = true;
  }

  //
  // If this backtrace is generated from sampling in a thread,
  // take off the top 'monitor_pthread_main' node
  //
  if (cct->ctxt && ! partial_unw && ! tramp_found && (bt.fence == FENCE_THREAD)) {
    TMSG(FENCE, "bt last thread correction made");
    TMSG(THREAD_CTXT, "Thread correction, back off outermost backtrace entry");
    bt_last--;
  }

  if (ENABLED(FLAT) && !isSync)
    bt_last = bt_beg;
  cct_node_t* n = hpcrun_cct_record_backtrace_w_metric(cct, partial_unw, bt.fence == FENCE_THREAD,
						       bt_beg, bt_last, tramp_found,
						       metricId, metricIncr);

  if (bt.trolled) hpcrun_stats_trolled_inc();
  hpcrun_stats_frames_total_inc((long)(bt.last - bt.begin + 1));
  hpcrun_stats_trolled_frames_inc((long) bt.n_trolls);

  if (ENABLED(USE_TRAMP)){
    TMSG(TRAMP, "--NEW SAMPLE--: Remove old trampoline");
    hpcrun_trampoline_remove();
    td->tramp_frame = td->cached_bt;
    TMSG(TRAMP, "--NEW SAMPLE--: Insert new trampoline");
    hpcrun_trampoline_insert(n);
  }

  return n;
}
