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
#include <hpcrun/unresolved.h>
#include <hpcrun/ompt-interface.h>
#include <lib/prof-lean/lush/lush-support.h>
#include <lib/prof-lean/placeholders.h>
#include <lush/lush-backtrace.h>
#include <thread_data.h>
#include <hpcrun_stats.h>
#include <trampoline/common/trampoline.h>
#include <utilities/ip-normalized.h>
#include <utilities/task-cntxt.h>
#include <utilities/defer-cntxt.h>
#include "frame.h"
#include <unwind/common/backtrace_info.h>
#include <unwind/common/fence_enum.h>
#include "cct_insert_backtrace.h"

#include <ompt.h>

//
// Misc externals (not in an include file)
//
extern bool hpcrun_inbounds_main(void* addr);

//
// local variable records the on/off state of
// special recursive compression:
//
static bool retain_recursion = false;

#ifdef OMPT_DEBUG
#define elide_debug_dump(t,i,o) stack_dump(t,i,o)

static
void stack_dump(char *tag, frame_t *inner, frame_t *outer) 
{
  EMSG("-----%s start", tag); 
  for (frame_t* x = inner; x <= outer; ++x) {
    void* ip;
    hpcrun_unw_get_ip_unnorm_reg(&(x->cursor), &ip);

    load_module_t* lm = hpcrun_loadmap_findById(x->ip_norm.lm_id);
    const char* lm_name = (lm) ? lm->name : "(null)";

    EMSG("ip = %p (%p), load module = %s", ip, x->ip_norm.lm_ip, lm_name);
  }
  EMSG("-----%s end\n", tag); 
}

#else
#define elide_debug_dump(t,i,o) 
#endif

static int
interval_contains(void *lower, void *upper, void *addr)
{
  uint64_t uaddr  = (uint64_t) addr;
  uint64_t ulower = (uint64_t) lower;
  uint64_t uupper = (uint64_t) upper;
  
  return ((ulower <= uaddr) & (uaddr <= uupper));
}


static void
hpcrun_elide_runtime_frame(frame_t **bt_outer, frame_t **bt_inner)
{
  int i = 0;
  frame_t *it = NULL;

  ompt_frame_t *frame0 = hpcrun_ompt_get_task_frame(i);

  //---------------------------------------------------------------
  // handle all of the corner cases that can occur at the top of 
  // the stack first
  //---------------------------------------------------------------

  if (!frame0) {
    // corner case: the innermost task (if any) has no frame info. 
    // no action necessary. just return.
    return;
  }

  if ((frame0->reenter_runtime_frame == 0) && (frame0->exit_runtime_frame == 0)) {
    // corner case: the top frame has been set up, but not filled in. 
    // ignore this frame.
    frame0 = hpcrun_ompt_get_task_frame(++i);
  }

  if (!frame0) {
    // corner case: the innermost task (if any) has no frame info. 
    // no action necessary. just return.
    return;
  }

  if (frame0->exit_runtime_frame && 
      (((uint64_t) frame0->exit_runtime_frame) < ((uint64_t) (*bt_inner)->cursor.sp))) {
    // corner case: the top frame has been set up, exit frame has been filled in; 
    // however, exit_runtime_frame points beyond the top of stack. the final call 
    // to user code hasn't been made yet. ignore this frame.
    frame0 = hpcrun_ompt_get_task_frame(++i);
  }

  elide_debug_dump("ORIGINAL", *bt_inner, *bt_outer); 
  
  if (frame0->reenter_runtime_frame) { 
    // the sample was received inside the runtime; 
    // elide frames from top of stack down to runtime entry
    int found = 0;
    for (it = *bt_inner; it <= *bt_outer; it++) {
      if ((uint64_t)(it->cursor.sp) >= (uint64_t)frame0->reenter_runtime_frame) {
	*bt_inner = it;
	found = 1;
	break;
      }
    }
    if (found == 0) {
#if 0
      /* eliminate all frames */
      *bt_inner = *bt_outer + 1;
#endif
      /* runtime frames with nothing else; it is harmless to reveal them all */

      elide_debug_dump("ELIDED INNERMOST FRAMES", *bt_inner, *bt_outer); 

      /* nothing left to do */
      return;
    }
    elide_debug_dump("ELIDED INNERMOST FRAMES", *bt_inner, *bt_outer); 
    /* frames at top of stack elided. move down one level */ 
  }

  // general case: elide frames between frame1->enter and frame0->exit
  while (true) {
    frame_t *exit0 = NULL, *reenter1 = NULL;
    ompt_frame_t *frame1;

    frame0 = hpcrun_ompt_get_task_frame(i);
    frame1 = hpcrun_ompt_get_task_frame(++i);

    if (!frame0 || !frame1) break;

    void *low_sp = (*bt_inner)->cursor.sp;
    void *high_sp = (*bt_outer)->cursor.sp;

    // if a frame marker is inside the call stack, set its flag to true
    bool exit0_flag = 
      interval_contains(low_sp, high_sp, frame0->exit_runtime_frame);

    bool reenter1_flag = 
      interval_contains(low_sp, high_sp, frame1->reenter_runtime_frame); 

    /* start from the top of the stack (innermost frame). 
       find the matching frame in the callstack for each of the markers in the
       stack. look for them in the order in which they should occur.

       optimization note: this always starts at the top of the stack. this can
       lead to quadratic cost. could pick up below where you left off cutting in 
       previous iterations.
    */
    it = *bt_inner; 
    if(exit0_flag) {
      for (; it <= *bt_outer; it++) {
        if((uint64_t)(it->cursor.sp) >= (uint64_t)(frame0->exit_runtime_frame)) {
          exit0 = it;
          break;
        }
      }
    }
    if(reenter1_flag) {
      for (; it <= *bt_outer; it++) {
        if((uint64_t)(it->cursor.sp) >= (uint64_t)(frame1->reenter_runtime_frame)) {
          reenter1 = it;
          break;
        }
      }
    }

    if (exit0 && reenter1) {
#if 0
      memmove(*bt_inner+(reenter1-exit0+1), *bt_inner, (exit0 - *bt_inner)*sizeof(frame_t));
      *bt_inner = *bt_inner + (reenter1 - exit0 + 1);
#else
      // was missing a frame with intel's runtime; eliminate +1 -- johnmc
      memmove(*bt_inner+(reenter1-exit0), *bt_inner, (exit0 - *bt_inner)*sizeof(frame_t));
      *bt_inner = *bt_inner + (reenter1 - exit0);
#endif
      exit0 = reenter1 = NULL;
    } else if (exit0 && !reenter1) {
      // corner case: reenter1 is in the team master's stack, not this one. eliminate all
      // frames below the exit frame.
      *bt_outer = exit0 - 1;
      break;
    }

    // inside a task, check one iteration is enough
    if(hpcrun_ompt_get_task_data(0) && hpcrun_ompt_get_task_data(0)->ptr) break;
  }

  elide_debug_dump("ELIDED", *bt_inner, *bt_outer); 
}

static cct_node_t*
cct_insert_raw_backtrace(cct_node_t* cct,
                            frame_t* path_beg, frame_t* path_end)
{
  if (hpcrun_ompt_elide_frames() && (path_beg < path_end) && cct) {
    // map the empty call path to omp_barrier to indicate an idle worker
    void *omp_barrier_addr = canonicalize_placeholder(omp_barrier);
    ip_normalized_t tmp_ip = hpcrun_normalize_ip(omp_barrier_addr, NULL);
    cct_addr_t tmp = ADDR2(tmp_ip.lm_id, tmp_ip.lm_ip);
    cct = hpcrun_cct_insert_addr(cct, &tmp);
    hpcrun_cct_terminate_path(cct);
    return cct;
  }

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
			  int skipInner, void *arg);

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
		     int skipInner, int isSync, void* arg)
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
				  skipInner, arg);
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

  TMSG(FENCE, "sanity check cursor = %p", cct_cursor);
  TMSG(FENCE, "further sanity check: bt_last frame = (%d, %p)", bt_last->ip_norm.lm_id, bt_last->ip_norm.lm_ip);
  return hpcrun_cct_insert_backtrace(cct_cursor, bt_last, bt_beg);

}

static cct_node_t *
memoized_context_get(thread_data_t* td, uint64_t region_id)
{
  return (td->outer_region_id == region_id && td->outer_region_context) ? 
    td->outer_region_context : 
    NULL;
}

static void
memoized_context_set(thread_data_t* td, uint64_t region_id, cct_node_t *result)
{
    td->outer_region_id = region_id;
    td->outer_region_context = result;
}

static cct_node_t *
lookup_region_id(cct_node_t **root, uint64_t region_id)
{
  thread_data_t* td = hpcrun_get_thread_data();
  cct_node_t *result = NULL;

  result = memoized_context_get(td, region_id);
  if (result) return result;
  
  cct_node_t *t0_path = hpcrun_region_lookup(region_id);
  if (t0_path) {
    result = hpcrun_cct_insert_path_return_leaf(*root, t0_path);
    memoized_context_set(td, region_id, result);
  }

  return result;
}

cct_node_t*
hpcrun_cct_record_backtrace_w_metric(cct_bundle_t* cct, bool partial, bool thread_stop,
				     frame_t* bt_beg, frame_t* bt_last, bool tramp_found,
				     int metricId, uint64_t metricIncr, void* arg)
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

  omp_arg_t* omp_arg = (omp_arg_t*) arg;

  // if deferred context is available, resolve it and update cursor accordingly
  if (arg && omp_arg->tbd) {
    cct_node_t *prefix = lookup_region_id(&cct->tree_root, omp_arg->region_id);
    if (prefix) cct_cursor = prefix;
    else {
      assert(omp_arg->should_resolve == 0);
      prefix = hpcrun_cct_find_addr((hpcrun_get_thread_epoch()->csdata).unresolved_root, 
				    &(ADDR2(UNRESOLVED, omp_arg->region_id)));
      if (prefix) cct_cursor = prefix;
    }
  }

  // this is for omp task
  if(arg && omp_arg->context) {
    cct_cursor = (cct_node_t *)omp_arg->context; 
    hack_task_context(&bt_beg, &bt_last);
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
						       metricId, metricIncr, NULL);

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
help_hpcrun_backtrace2cct(cct_bundle_t* bundle, ucontext_t* context,
			  int metricId,
			  uint64_t metricIncr,
			  int skipInner, void* arg)
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

#if 0
  //
  // If this backtrace is generated from sampling in a thread,
  // take off the top 'monitor_pthread_main' node
  //
  if (bundle->ctxt && ! partial_unw && ! tramp_found && (bt.fence == FENCE_THREAD)) {
    TMSG(FENCE, "bt last thread correction made");
    TMSG(THREAD_CTXT, "Thread correction, back off outermost backtrace entry");
    bt_last--;
    bt_last--;
  }
#endif

#define INTEL_RTL 1

  // ompt: elide runtime frames
  omp_arg_t* omp_arg = (omp_arg_t*) arg;
  if (hpcrun_ompt_elide_frames() && 
      ((bt.fence == FENCE_MAIN) ||            // always elide in the main thread
       (omp_arg && omp_arg->region_id != 0))  // elide in the side thread if appropriate
      ) {
    frame_t* bt_last_orig = bt_last;
    hpcrun_elide_runtime_frame(&bt_last, &bt_beg);
    if ((bt.fence == FENCE_THREAD) && (bt_last_orig == bt_last)) {
      // no elision was performed on a side thread. they are all runtime frames 
#if INTEL_RTL
      {
	// this backtrace can be related to the global view
	// pop off the bottom three frames of the intel runtime
	bt_last = bt_last - 3;
	omp_arg->should_resolve = hpcrun_trace_isactive();
      }
#endif
    }
  } 
#ifdef DEBUG_ELIDE
  else {
    int region_id = omp_arg ? omp_arg->region_id : -1;
    EMSG("not eliding in side thread: elide_frames=%d omp_arg=%p omp_arg->region_id=%d\n", 
	 hpcrun_ompt_elide_frames(), omp_arg, region_id);
  }
#endif

    // master thread elides all frames (to be omp_barrier), then make it as partial to 
    // avoid monitor_main and omp_barrier appearing in the CCT(s)

  if((bt_last < bt_beg) && (bt.fence == FENCE_MAIN)) {
    EMSG("main fence partial");
    partial_unw = true;
  }

#if 0
  if((bt_last < bt_beg) && (bt.fence == FENCE_THREAD)) {
    EMSG("thread fence partial");
    partial_unw = true;
  }
#endif

  cct_node_t* n = hpcrun_cct_record_backtrace_w_metric(bundle, partial_unw, bt.fence == FENCE_THREAD,
						       bt_beg, bt_last, tramp_found,
						       metricId, metricIncr, arg);

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
