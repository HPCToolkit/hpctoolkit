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
#include "frame.h"

static cct_node_t*
cct_insert_raw_backtrace(cct_node_t* cct,
                            frame_t* path_beg, frame_t* path_end)
{
  if ( (path_beg < path_end) || (!cct)) {
    return NULL;
  }
  for(; path_beg >= path_end; path_beg--){
    cct_addr_t tmp = (cct_addr_t) {.as_info = path_beg->as_info, .ip_norm = path_beg->ip_norm, .lip = path_beg->lip};
    cct = hpcrun_cct_insert_addr(cct, &tmp);
  }
  return cct;
}

static cct_node_t*
help_hpcrun_backtrace2cct(cct_bundle_t* cct, ucontext_t* context,
			  int metricId, uint64_t metricIncr,
			  int skipInner);

// See usage in header.
cct_node_t*
hpcrun_cct_insert_backtrace(cct_bundle_t* cct, cct_node_t* treenode,
			    int metric_id,
			    frame_t* path_beg, frame_t* path_end,
			    cct_metric_data_t datum)
{
  if (! treenode) treenode = cct->tree_root;

  cct_node_t* path = cct_insert_raw_backtrace(treenode, path_beg, path_end);

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

  hpcrun_get_metric_proc(metric_id)(metric_id, &(hpcrun_cct_metrics(path)[metric_id]), datum);
  return path;
}

//
// Insert new backtrace in cct
//

cct_node_t*
hpcrun_cct_insert_bt(cct_bundle_t* cct, cct_node_t* node,
		     int metricId,
		     backtrace_t* bt,
		     cct_metric_data_t datum)
{
  return hpcrun_cct_insert_backtrace(cct, node, metricId, hpcrun_bt_last(bt), hpcrun_bt_beg(bt), datum);
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
    TMSG(LUSH,"regular (NON-lush) backtrace2cct invoked");
    n = help_hpcrun_backtrace2cct(cct, context, metricId, metricIncr, skipInner);
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
hpcrun_cct_record_backtrace(cct_bundle_t* cct, bool partial,
			    frame_t* bt_beg, frame_t* bt_last, bool tramp_found,
			    int metricId, uint64_t metricIncr)
{
  thread_data_t* td = hpcrun_get_thread_data();
  cct_node_t* cct_cursor = NULL;
  if (tramp_found) {
    // start insertion below caller's frame, which is marked with the trampoline
    cct_cursor = hpcrun_cct_parent(td->tramp_cct_node);
  }
  if (partial) {
    cct_cursor = cct->partial_unw_root;
  }

  return hpcrun_cct_insert_backtrace(cct, cct_cursor, metricId,
				     bt_last, bt_beg,
				     (cct_metric_data_t){.i = metricIncr});

}

cct_node_t*
hpcrun_dbg_backtrace2cct(cct_bundle_t* cct, ucontext_t* context,
			 int metricId,
			 uint64_t metricIncr,
			 int skipInner)
{
  bool tramp_found;
  frame_t* bt_beg;
  frame_t* bt_last;
  thread_data_t* td = hpcrun_get_thread_data();

  if (! hpcrun_dbg_generate_graceful_backtrace(context,
					       &bt_beg, &bt_last, &tramp_found,
					       skipInner)) {
    if (ENABLED(NO_PARTIAL_UNW)){
      return NULL;
    }
    else {
      TMSG(PARTIAL_UNW, "recording partial unwind from graceful failure");
      hpcrun_stats_num_samples_partial_inc();
    }
  }

  cct_node_t* n = hpcrun_cct_record_backtrace(cct, true,
					      bt_beg, bt_last, tramp_found,
					      metricId, metricIncr);

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
			  int skipInner)
{
  bool tramp_found;
  frame_t* bt_beg;
  frame_t* bt_last;
  thread_data_t* td = hpcrun_get_thread_data();

  bool partial_unw = false;
  if (! hpcrun_generate_backtrace(context,
				  &bt_beg, &bt_last, &tramp_found,
				  skipInner)) {
    if (ENABLED(NO_PARTIAL_UNW)){
      return NULL;
    }
    TMSG(PARTIAL_UNW, "recording partial unwind from graceful failure");
    hpcrun_stats_num_samples_partial_inc();
    partial_unw = true;
  }

  //
  // If this backtrace is generated from sampling in a thread,
  // take off the top 'monitor_pthread_main' node
  //
  if (cct->ctxt) {
      bt_last--;
  }
  cct_node_t* n = hpcrun_cct_record_backtrace(cct, partial_unw,
					      bt_beg, bt_last, tramp_found,
					      metricId, metricIncr);

  if (ENABLED(USE_TRAMP)){
    hpcrun_trampoline_remove();
    td->tramp_frame = td->cached_bt;
    hpcrun_trampoline_insert(n);
  }

  return n;
}
