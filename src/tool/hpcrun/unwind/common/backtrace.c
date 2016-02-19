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


//***************************************************************************
// system include files
//***************************************************************************

#include <stdbool.h>
#include <assert.h>
#include <stdlib.h>

#include <sys/types.h>
#include <ucontext.h>

#include <string.h>


//***************************************************************************
// local include files
//***************************************************************************

#include <cct/cct.h>

#include <unwind/common/unwind.h>
#include <unwind/common/backtrace.h>
#include <unwind/common/unw-throw.h>
#include <hpcrun/hpcrun_stats.h>
#include <hpcrun/thread_data.h>

#include <hpcrun/epoch.h>
#include <monitor.h>
#include <hpcrun/sample_event.h>
#include <fnbounds/fnbounds_interface.h>

#include <messages/messages.h>
#include <lush/lush-backtrace.h>
#include <trampoline/common/trampoline.h>
#include <memory/hpcrun-malloc.h>
#include <dbg_backtrace.h>

extern void hpcrun_trampoline(void); // trampoline function, needed only for debug

//***************************************************************************
// local constants & macros
//***************************************************************************

//***************************************************************************
// forward declarations 
//***************************************************************************

static void lush_assoc_info2str(char* buf, size_t len, lush_assoc_info_t info);
static void lush_lip2str(char* buf, size_t len, lush_lip_t* lip);

//***************************************************************************
// interface functions
//***************************************************************************

void
hpcrun_show_backtrace(char* label, frame_t* beg, frame_t* end)
{
  ;
}

void 
hpcrun_bt_dump(frame_t* unwind, const char* tag)
{
  static const int msg_limit = 100;
  int msg_cnt = 0;

  char as_str[LUSH_ASSOC_INFO_STR_MIN_LEN];
  char lip_str[LUSH_LIP_STR_MIN_LEN];

  const char* mytag = (tag) ? tag : "";
  EMSG("-- begin new backtrace (innermost first) [%s] ----------", mytag);

  thread_data_t* td = hpcrun_get_thread_data();
  if (unwind) {
    for (frame_t* x = td->btbuf_beg; x < unwind; ++x) {
      lush_assoc_info2str(as_str, sizeof(as_str), x->as_info);
      lush_lip2str(lip_str, sizeof(lip_str), x->lip);

      void* ip;
      hpcrun_unw_get_ip_unnorm_reg(&(x->cursor), &ip);

      load_module_t* lm = hpcrun_loadmap_findById(x->ip_norm.lm_id);
      const char* lm_name = (lm) ? lm->name : "(null)";

      EMSG("%s: ip = %p (%p), load module = %s | lip %s", as_str, ip, x->ip_norm.lm_ip, lm_name, lip_str);

      msg_cnt++;
      if (msg_cnt > msg_limit) {
        EMSG("!!! message limit !!!");
        break;
      }
    }
  }

  if (msg_cnt <= msg_limit && td->btbuf_sav != td->btbuf_end) {
    EMSG("-- begin cached backtrace ---------------------------");
    for (frame_t* x = td->btbuf_sav; x < td->btbuf_end; ++x) {
      lush_assoc_info2str(as_str, sizeof(as_str), x->as_info);
      lush_lip2str(lip_str, sizeof(lip_str), x->lip);
      EMSG("%s: ip.lm_id = %d | ip.lm_ip = %p | lip %s", as_str,
	   x->ip_norm.lm_id, x->ip_norm.lm_ip, lip_str);
      msg_cnt++;
      if (msg_cnt > msg_limit) {
        EMSG("!!! message limit !!!");
        break;
      }
    }
  }

  EMSG("-- end backtrace ------------------------------------\n");
}

frame_t*
hpcrun_bt_reset(backtrace_t* bt)
{
  bt->cur = bt->beg;
  bt->len = 0;
  return bt->cur;
}

void
hpcrun_bt_init(backtrace_t* bt, size_t size)
{
  bt->beg   = (frame_t*) hpcrun_malloc(sizeof(frame_t) * size);
  bt->end   = bt->beg + (size - 1);
  bt->size  = size;
  hpcrun_bt_reset(bt);
}

frame_t*
hpcrun_bt_push(backtrace_t* bt, frame_t* frame)
{
  TMSG(BT, "pushing frame onto bt");
  if (bt->cur > bt->end) {
    
    TMSG(BT, "-- push requires allocate new block and copy");
    size_t size = 2 * bt->size;
    // reallocate & copy
    frame_t* new = hpcrun_malloc(sizeof(frame_t) * size);
    memcpy(new, (void*) bt->beg, bt->len * sizeof(frame_t));

    bt->beg  = new;
    bt->size = size;
    bt->cur  = new + bt->len;
    bt->end  = new + (size - 1);
  }
  *(bt->cur) = *frame;
  frame_t* rv = bt->cur;
  bt->cur++;
  bt->len++;
  TMSG(BT, "after push, len(bt) = %d", bt->len);
  return rv;
}

frame_t*
hpcrun_bt_beg(backtrace_t* bt)
{
  return bt->beg;
}

frame_t*
hpcrun_bt_last(backtrace_t* bt)
{
  return (bt->beg + bt->len -1);
}

size_t
hpcrun_bt_len(backtrace_t* bt)
{
  return bt->len;
}

bool
hpcrun_bt_empty(backtrace_t* bt)
{
  return (bt->len == 0);
}

frame_t*
hpcrun_bt_cur(backtrace_t* bt)
{
  return bt->cur;
}

//
// Some sample backtrace mutator functions
//

void
hpcrun_bt_modify_leaf_addr(backtrace_t* bt, ip_normalized_t ip_norm)
{
  bt->beg->ip_norm = ip_norm;
}

void
hpcrun_bt_add_leaf_child(backtrace_t* bt, ip_normalized_t ip_norm)
{
  if (bt->cur > bt->end) {
    TMSG(BT, "adding a leaf child of ip ==> lm_id = %d and lm_ip = %p", 
	 ip_norm.lm_id, ip_norm.lm_ip);
  }
  if (bt->cur > bt->end) {
    
    TMSG(BT, "-- bt is full, reallocate and copy current data");
    size_t size = 2 * bt->size;
    // reallocate & copy
    frame_t* new = hpcrun_malloc(sizeof(frame_t) * size);
    memmove(new, (void*) bt->beg, bt->len * sizeof(frame_t));

    bt->beg  = new;
    bt->size = size;
    bt->cur  = new + bt->len;
    bt->end  = new + (size - 1);
  }
  TMSG(BT, "BEFORE copy, innermost ip ==> lm_id = %d and lm_ip = %p", 
       bt->beg->ip_norm.lm_id, bt->beg->ip_norm.lm_ip);
  memcpy((void*)(bt->beg + 1), (void*) bt->beg, bt->len * sizeof(frame_t));
  TMSG(BT, "AFTER copy, innermost ip ==> lm_id = %d and lm_ip = %p", 
       (bt->beg + 1)->ip_norm.lm_id, (bt->beg + 1)->ip_norm.lm_ip);
  bt->cur++;
  bt->len++;
  bt->beg->ip_norm = ip_norm;
  TMSG(BT, "Leaf child added, new ip ==> lm_id = %d and lm_ip = %p", 
       bt->beg->ip_norm.lm_id, bt->beg->ip_norm.lm_ip);
}

void
hpcrun_bt_skip_inner(backtrace_t* bt, void* skip)
{
  size_t realskip = (size_t) skip;
  bt->beg += realskip;
}

void
hpcrun_dump_bt(backtrace_t* bt)
{
  for(frame_t* _f = bt->beg; _f < bt->beg + bt->len; _f++) {
    TMSG(BT, "ip_norm.lm_id = %d, and ip_norm.lm_ip = %p ", _f->ip_norm.lm_id,
	 _f->ip_norm.lm_ip);
  }
}

//---------------------------------------------------------------------------
// function: hpcrun_filter_sample
//
// purpose:
//     ignore any samples that aren't rooted at designated call sites in 
//     monitor that should be at the base of all process and thread call 
//     stacks. 
//
// implementation notes:
//     to support this, in monitor we define a pair of labels surrounding the 
//     call site of interest. it is possible to get a sample between the pair 
//     of labels that is outside the call. in that case, the length of the 
//     sample's callstack would be 1, and we ignore it.
//-----------------------------------------------------------------------------
bool
hpcrun_filter_sample(int len, frame_t* start, frame_t* last)
{
  void* ip_unnorm;
  hpcrun_unw_get_ip_unnorm_reg(&last->cursor, &ip_unnorm);
  return ( !(monitor_in_start_func_narrow(ip_unnorm)
	     && (len > 1)) );
}

frame_t*
hpcrun_skip_chords(frame_t* bt_outer, frame_t* bt_inner, 
		   int skip)
{
  // N.B.: INVARIANT: bt_inner < bt_outer
  frame_t* x_inner = bt_inner;
  for (int i = 0; (x_inner < bt_outer && i < skip); ++i) {
    // for now, do not support M chords
    lush_assoc_t as = lush_assoc_info__get_assoc(x_inner->as_info);
    assert(as == LUSH_ASSOC_NULL || as == LUSH_ASSOC_1_to_1 ||
	   as == LUSH_ASSOC_1_to_0);
    
    x_inner++;
  }
  
  return x_inner;
}

//
// Generate a backtrace, store it in the thread local data
// Return true/false success code
// Also, return (via reference params) backtrace beginning, backtrace end,
//  and whether or not a trampoline was found.
//
// NOTE: This routine will stop the backtrace if a trampoline is encountered,
//       but it will NOT update the trampoline data structures.
//
bool
hpcrun_generate_backtrace_no_trampoline(backtrace_info_t* bt,
					ucontext_t* context,
					int skipInner)
{
  TMSG(BT, "Generate backtrace (no tramp), skip inner = %d", skipInner);
  bt->has_tramp = false;
  bt->trolled  = false;
  bt->n_trolls = 0;
  bt->fence = FENCE_BAD;
  bt->bottom_frame_elided = false;
  bt->partial_unwind = true;

  bool tramp_found = false;

  step_state ret = STEP_ERROR; // default return value from stepper

  //--------------------------------------------------------------------
  // note: these variables are not local variables so that if a SIGSEGV 
  // occurs and control returns up several procedure frames, the values 
  // are accessible to a dumping routine that will tell us where we ran 
  // into a problem.
  //--------------------------------------------------------------------

  thread_data_t* td = hpcrun_get_thread_data();
  td->btbuf_cur   = td->btbuf_beg; // innermost
  td->btbuf_sav   = td->btbuf_end;

  hpcrun_unw_cursor_t cursor;
  hpcrun_unw_init_cursor(&cursor, context);

  int unw_len = 0;
  while (true) {
    void* ip;
    hpcrun_unw_get_ip_unnorm_reg(&cursor, &ip);

    if (hpcrun_trampoline_interior((void*) ip)) {
      // bail; we shouldn't be unwinding here. hpcrun is in the midst of 
      // counting a return from a sampled frame using a trampoline.
      // drop the sample. 
      // FIXME: sharpen the information to indicate why the sample is 
      //        being dropped.
      hpcrun_unw_drop();
    }

    if (hpcrun_trampoline_at_entry((void*) ip)) {
      if (unw_len == 0){
	// we are about to enter the trampoline code to synchronously 
	// record a return. for now, simply do nothing ...
	// FIXME: with a bit more effort, we could charge 
	//        the sample to the return address in the caller. 
	hpcrun_unw_drop();
      }
      else {
	// we have encountered a trampoline in the middle of an unwind.
	bt->has_tramp = (tramp_found = true);
	TMSG(TRAMP, "--CURRENT UNWIND FINDS TRAMPOLINE @ (sp:%p, bp:%p", cursor.sp, cursor.bp);
	// no need to unwind further. the outer frames are already known.

	bt->fence = FENCE_TRAMP;
	ret = STEP_STOP;
	break;
      }
    }
    
    hpcrun_ensure_btbuf_avail();

    td->btbuf_cur->cursor = cursor;
    //Broken if HPC_UNW_LITE defined
    hpcrun_unw_get_ip_norm_reg(&td->btbuf_cur->cursor,
			       &td->btbuf_cur->ip_norm);
    td->btbuf_cur->ra_loc = NULL;

    void *func_start_pc = NULL, *func_end_pc = NULL;
    load_module_t* lm = NULL;
    fnbounds_enclosing_addr(cursor.pc_unnorm, &func_start_pc, &func_end_pc, &lm);
    td->btbuf_cur->the_function = hpcrun_normalize_ip(func_start_pc, lm);

    frame_t* prev = td->btbuf_cur;
    td->btbuf_cur++;
    unw_len++;

    ret = hpcrun_unw_step(&cursor);
    if (ret == STEP_TROLL) {
      bt->trolled = true;
      bt->n_trolls++;
    }
    if (ret <= 0) {
      if (ret == STEP_ERROR) {
	hpcrun_stats_num_samples_dropped_inc();
      }
      else { // STEP_STOP
	bt->fence = cursor.fence;
      }
      break;
    }
    prev->ra_loc = hpcrun_unw_get_ra_loc(&cursor);
  }

  TMSG(FENCE, "backtrace generation detects fence = %s", fence_enum_name(bt->fence));

  frame_t* bt_beg  = td->btbuf_beg;      // innermost, inclusive
  frame_t* bt_last = td->btbuf_cur - 1; // outermost, inclusive

  if (skipInner) {
    if (ENABLED(USE_TRAMP)){
      //
      // FIXME: For the moment, ignore skipInner issues with trampolines.
      //        Eventually, this will need to be addressed
      //
      TMSG(TRAMP, "WARNING: backtrace detects skipInner != 0 (skipInner = %d)", 
	   skipInner);
    }
    TMSG(BT, "* BEFORE Skip inner correction, bt_beg = %p", bt_beg);
    // adjust the returned backtrace according to the skipInner
    bt_beg = hpcrun_skip_chords(bt_last, bt_beg, skipInner);
    TMSG(BT, "* AFTER Skip inner correction, bt_beg = %p", bt_beg);
  }

  bt->begin = bt_beg;         // returned backtrace begin
                              // is buffer beginning
  bt->last  = bt_last;        // returned backtrace last is
                             // last recorded element
  // soft error mandates returning false
  if (! (ret == STEP_STOP)) {
    TMSG(BT, "** Soft Failure **");
    return false;
  }

  TMSG(BT, "succeeds");
  bt->partial_unwind = false;
  return true;
}

//
// Do all of the raw backtrace generation, plus
// update the trampoline cached backtrace.
//
bool
hpcrun_generate_backtrace(backtrace_info_t* bt,
			  ucontext_t* context, int skipInner)
{
  bool ret = hpcrun_generate_backtrace_no_trampoline(bt,
						     context,
						     skipInner);
  if (! ret ) return false;

  thread_data_t* td = hpcrun_get_thread_data();

  bool tramp_found = bt->has_tramp;
  frame_t* bt_beg  = bt->begin;
  frame_t* bt_last = bt->last;

  frame_t* bt_end  = bt_last + 1;    // outermost, exclusive
  size_t new_frame_count = bt_end - bt_beg;

  if (ENABLED(USE_TRAMP)) {
    if (tramp_found) {
      TMSG(BACKTRACE, "tramp stop: conjoining backtraces");
      TMSG(TRAMP, " FOUND TRAMP: constructing cached backtrace");
      //
      // join current backtrace fragment to previous trampoline-marked prefix
      // and make this new conjoined backtrace the cached-backtrace
      //
      frame_t* prefix = td->tramp_frame;
      TMSG(TRAMP, "Check: tramp prefix ra_loc = %p, addr@ra_loc = %p (?= %p tramp), retn_addr = %p",
	   prefix->ra_loc, *((void**) prefix->ra_loc), hpcrun_trampoline, td->tramp_retn_addr);
      size_t old_frame_count = td->cached_bt_end - prefix;
      TMSG(TRAMP, "Check: Old frame count = %d ?= %d (computed frame count)",
	   old_frame_count, td->cached_frame_count);

      size_t n_cached_frames = new_frame_count - 1;
      hpcrun_cached_bt_adjust_size(n_cached_frames + old_frame_count);
      TMSG(TRAMP, "cached trace size = (new frames) %d + (old frames) %d = %d",
	   n_cached_frames, old_frame_count, n_cached_frames + old_frame_count);
      // put the old prefix in place
      memmove(td->cached_bt + n_cached_frames, prefix,
	      sizeof(frame_t) * old_frame_count);

      // put the new suffix in place
      memcpy(td->cached_bt, bt_beg, sizeof(frame_t) * n_cached_frames);

      // update the length of the conjoined backtrace
      td->cached_bt_end = td->cached_bt + n_cached_frames + old_frame_count;
      // maintain invariants
      td->cached_frame_count = n_cached_frames + old_frame_count;
      td->tramp_frame = td->cached_bt + n_cached_frames;
      TMSG(TRAMP, "Check: tramp prefix ra_loc = %p, addr@ra_loc = %p (?= %p tramp), retn_addr = %p",
	   prefix->ra_loc, *((void**) prefix->ra_loc), hpcrun_trampoline, td->tramp_retn_addr);
    }
    else {
      TMSG(TRAMP, "No tramp found: cached backtrace size = %d", new_frame_count);
      hpcrun_cached_bt_adjust_size(new_frame_count);
      size_t n_cached_frames = new_frame_count - 1;
      TMSG(TRAMP, "Confirm: ra_loc(last bt frame) = %p", (bt_beg + n_cached_frames)->ra_loc);
      memmove(td->cached_bt, bt_beg, sizeof(frame_t) * n_cached_frames);

      td->cached_bt_end = td->cached_bt + n_cached_frames;
      td->cached_frame_count = n_cached_frames;
    }
    if (ENABLED(TRAMP)) {
      TMSG(TRAMP, "Dump cached backtrace from backtrace construction");
      hpcrun_trampoline_bt_dump();
    }
  }

  return true;
}

// debug variant of above routine
//
//
// Generate a backtrace, store it in the thread local data
// Return true/false success code
// Also, return (via reference params) backtrace beginning, backtrace end,
//  and whether or not a trampoline was found.
//

bool
hpcrun_dbg_generate_backtrace(backtrace_info_t* bt,
			      ucontext_t* context, int skipInner)
{
  bt->has_tramp = false;
  bt->trolled  = false;
  bt->n_trolls = 0;
  bt->bottom_frame_elided = false;
  bt->partial_unwind = true;

  bool tramp_found = false;

  step_state ret = STEP_ERROR; // default return value from stepper

  hpcrun_unw_cursor_t cursor;
  hpcrun_dbg_unw_init_cursor(&cursor, context);

  //--------------------------------------------------------------------
  // note: these variables are not local variables so that if a SIGSEGV 
  // occurs and control returns up several procedure frames, the values 
  // are accessible to a dumping routine that will tell us where we ran 
  // into a problem.
  //--------------------------------------------------------------------

  thread_data_t* td = hpcrun_get_thread_data();
  td->btbuf_cur   = td->btbuf_beg; // innermost
  td->btbuf_sav   = td->btbuf_end;

  int unw_len = 0;
  while (true) {
    void* ip;
    hpcrun_unw_get_ip_unnorm_reg(&cursor, &ip);

    if (hpcrun_dbg_trampoline_interior((void*) ip)) {
      // bail; we shouldn't be unwinding here. hpcrun is in the midst of 
      // counting a return from a sampled frame using a trampoline.
      // drop the sample. 
      // FIXME: sharpen the information to indicate why the sample is 
      //        being dropped.
      hpcrun_unw_throw();
    }

    if (hpcrun_dbg_trampoline_at_entry((void*) ip)) {
      if (unw_len == 0){
	// we are about to enter the trampoline code to synchronously 
	// record a return. for now, simply do nothing ...
	// FIXME: with a bit more effort, we could charge 
	//        the sample to the return address in the caller. 
	hpcrun_unw_throw();
      }
      else {
	// we have encountered a trampoline in the middle of an unwind.
	bt->has_tramp = (tramp_found = true);
	
	// no need to unwind further. the outer frames are already known.
	break;
      }
    }
    
    hpcrun_ensure_btbuf_avail();

    td->btbuf_cur->cursor = cursor;
    //Broken if HPC_UNW_LITE defined
    hpcrun_unw_get_ip_norm_reg(&td->btbuf_cur->cursor,
			       &td->btbuf_cur->ip_norm);
    td->btbuf_cur->ra_loc = NULL;
    frame_t* prev = td->btbuf_cur;
    td->btbuf_cur++;
    unw_len++;

    ret = hpcrun_dbg_unw_step(&cursor);
    if (ret == STEP_TROLL) {
      bt->trolled = true;
      bt->n_trolls++;
    }
    if (ret <= 0) {
      break;
    }
    prev->ra_loc = hpcrun_unw_get_ra_loc(&cursor);
  }

  frame_t* bt_beg  = td->btbuf_beg;      // innermost, inclusive
  frame_t* bt_last = td->btbuf_cur - 1; // outermost, inclusive

  bt->begin        = bt_beg;         // returned backtrace begin
                                     // is buffer beginning
  bt->last         = bt_last;        // returned backtrace end is
                                     // last recorded element

  frame_t* bt_end  = bt_last + 1;    // outermost, exclusive
  size_t new_frame_count = bt_end - bt_beg;

  // soft error mandates returning false
  if (! (ret == STEP_STOP)) {
    return false;
  }

  if (tramp_found) {
    TMSG(BACKTRACE, "tramp stop: conjoining backtraces");
    //
    // join current backtrace fragment to previous trampoline-marked prefix
    // and make this new conjoined backtrace the cached-backtrace
    //
    frame_t* prefix = td->tramp_frame + 1; // skip top frame
    size_t old_frame_count = td->cached_bt_end - prefix;

    hpcrun_cached_bt_adjust_size(new_frame_count + old_frame_count);

    // put the old prefix in place
    memmove(td->cached_bt + new_frame_count, prefix, 
	   sizeof(frame_t) * old_frame_count);

    // put the new suffix in place
    memcpy(td->cached_bt, bt_beg, sizeof(frame_t) * new_frame_count);

    // update the length of the conjoined backtrace
    td->cached_bt_end = td->cached_bt + new_frame_count + old_frame_count;
  }
  else {
    hpcrun_cached_bt_adjust_size(new_frame_count);
    memmove(td->cached_bt, bt_beg, sizeof(frame_t) * new_frame_count);

    td->cached_bt_end = td->cached_bt + new_frame_count;
  }

  if (skipInner) {
    if (ENABLED(USE_TRAMP)){
      //
      // FIXME: For the moment, ignore skipInner issues with trampolines.
      //        Eventually, this will need to be addressed
      //
      EMSG("WARNING: backtrace detects skipInner != 0 (skipInner = %d)", 
	   skipInner);
    }
    // adjust the returned backtrace according to the skipInner
    bt->begin = hpcrun_skip_chords(bt_last, bt_beg, skipInner);
  }

  bt->partial_unwind = false;
  return true;
}

#if 0
bool
hpcrun_dbg_generate_backtrace(backtrace_info_t* bt,
			      ucontext_t* context, int skipInner)
{
  thread_data_t* td = hpcrun_get_thread_data();

  if (td->debug1) {
    EMSG("2nd call to generated failing backtrace causes exit");
    exit(0);
  }
  EMSG("Failing backtrace simulated");
  td->debug1 = true;

  bool rv = hpcrun_generate_backtrace(bt, context, skipInner);
  if (!rv) return false;

  size_t len = bt->last - bt->begin + 1;
  EMSG("Length of recorded backtrace = %d", len);
  bt->last -= (len > 2) ? 2 : 1;
  bt->has_tramp   = false;

  return false;
}
#endif // 0 for old dbg backtrace

//
// generate a backtrace, store it in thread-local data
// return success/failure
//
bool
hpcrun_gen_bt(ucontext_t* context, bool* has_tramp,
	      bt_mut_fn bt_fn, bt_fn_arg bt_arg)
{
  int  backtrace_trolled = 0;
  bool tramp_found       = false;

  hpcrun_unw_cursor_t cursor;
  hpcrun_unw_init_cursor(&cursor, context);

  //--------------------------------------------------------------------
  // note: these variables are not local variables so that if a SIGSEGV 
  // occurs and control returns up several procedure frames, the values 
  // are accessible to a dumping routine that will tell us where we ran 
  // into a problem.
  //--------------------------------------------------------------------

  thread_data_t* td = hpcrun_get_thread_data();

  backtrace_t* bt = &(td->bt);
  hpcrun_bt_reset(bt);

  int ret = STEP_ERROR;
  while (true) {

    void* ip = 0;
    hpcrun_unw_get_ip_unnorm_reg(&cursor, &ip);

    if (hpcrun_trampoline_interior((void*) ip)) {
      // bail; we shouldn't be unwinding here. hpcrun is in the midst of 
      // counting a return from a sampled frame using a trampoline.
      // drop the sample. 
      // FIXME: sharpen the information to indicate why the sample is 
      //        being dropped.
      hpcrun_unw_throw();
    }

    if (hpcrun_trampoline_at_entry((void*) ip)) {
      if (hpcrun_bt_len(bt) == 0){
	// we are about to enter the trampoline code to synchronously 
	// record a return. for now, simply do nothing ...
	// FIXME: with a bit more effort, we could charge 
	//        the sample to the return address in the caller. 
	hpcrun_unw_throw();
      }
      else {
	// we have encountered a trampoline in the middle of an unwind.
	tramp_found = true;

	// no need to unwind further. the outer frames are already known.
	break;
      }
    }
    
    ip_normalized_t ip_norm = hpcrun_normalize_ip((void*) ip,
						  cursor.intvl->lm);
    frame_t* prev = hpcrun_bt_push(bt,
				   &((frame_t){.cursor = cursor, 
					       .ip_norm = ip_norm,
					       .ra_loc = NULL}));
    
    ret = hpcrun_unw_step(&cursor);
    backtrace_trolled = backtrace_trolled || (ret == STEP_TROLL);
    if (ret <= 0) {
      break;
    }
    prev->ra_loc = hpcrun_unw_get_ra_loc(&cursor);
  }

  if (backtrace_trolled || (ret == STEP_ERROR)){
    hpcrun_up_pmsg_count();
  }
				    
  frame_t* bt_beg  = hpcrun_bt_beg(bt);

  size_t new_frame_count = hpcrun_bt_len(bt);

  if (tramp_found) {
    TMSG(BACKTRACE, "tramp stop: conjoining backtraces");
    //
    // join current backtrace fragment to previous trampoline-marked prefix
    // and make this new conjoined backtrace the cached-backtrace
    //
    frame_t* prefix = td->tramp_frame + 1; // skip top frame
    size_t old_frame_count = td->cached_bt_end - prefix;

    hpcrun_cached_bt_adjust_size(new_frame_count + old_frame_count);

    // put the old prefix in place
    memmove(td->cached_bt + new_frame_count, prefix, 
	   sizeof(frame_t) * old_frame_count);

    // put the new suffix in place
    memcpy(td->cached_bt, bt_beg, sizeof(frame_t) * new_frame_count);

    // update the length of the conjoined backtrace
    td->cached_bt_end = td->cached_bt + new_frame_count + old_frame_count;
  }
  else {
    hpcrun_cached_bt_adjust_size(new_frame_count);
    memmove(td->cached_bt, bt_beg, sizeof(frame_t) * new_frame_count);

    td->cached_bt_end = td->cached_bt + new_frame_count;
  }

  // let clients know if a trampoline was found or not
  *has_tramp = tramp_found;

#if 0 // no sample filtering
  if (! ENABLED(NO_SAMPLE_FILTERING)) {
    frame_t* beg_frame  = td->cached_bt;
    frame_t* last_frame = td->cached_bt_end - 1;
    int num_frames      = last_frame - beg_frame + 1;
	
    if (hpcrun_filter_sample(num_frames, beg_frame, last_frame)){
      TMSG(SAMPLE_FILTER, "filter sample of length %d", num_frames);
      frame_t *fr = beg_frame;
      for (int i = 0; i < num_frames; i++, fr++){
	TMSG(SAMPLE_FILTER,"  frame ip[%d] ==> lm_id = %d and lm_ip = %p", i, fr->ip_norm.lm_id, fr->ip_norm.lm_ip);
      }
      hpcrun_stats_num_samples_filtered_inc();
      return false;
    }
  }
#endif // no sample filtering
  //
  // mutate the backtrace according to the passed in mutator function
  //  (bt_fn == NULL means no mutation is necessary)
  //
  if (bt_fn) {
    TMSG(BT, "Mutation function called");
    TMSG(BT,"==== backtrace BEFORE mutation ========");
    hpcrun_dump_bt(bt);
    TMSG(BT,"-------------------------------");
    bt_fn(bt, bt_arg);
    TMSG(BT,"==== backtrace AFTER  mutation ========");
    hpcrun_dump_bt(bt);
    TMSG(BT,"-------------------------------");
  }

  return true;
}

//***************************************************************************
// private operations 
//***************************************************************************

static void
lush_assoc_info2str(char* buf, size_t len, lush_assoc_info_t info)
{
  // INVARIANT: buf must have at least LUSH_ASSOC_INFO_STR_MIN_LEN slots

  lush_assoc_t as = info.u.as;
  unsigned info_len = info.u.len;

  const char* as_str = lush_assoc_tostr(as);
  hpcrun_msg_ns(buf, LUSH_ASSOC_INFO_STR_MIN_LEN, "%s (%u)", as_str, info_len);
  buf[LUSH_ASSOC_INFO_STR_MIN_LEN - 1] = '\0';
}

static void
lush_lip2str(char* buf, size_t len, lush_lip_t* lip)
{
  *buf = '\0';

  if (lip) {
    for (int i = 0; i < LUSH_LIP_DATA8_SZ; ++i) {
      if (i != 0) {
	*(buf++) = ' ';
	*(buf) = '\0';
	len--;
      }
      int num = hpcrun_msg_ns(buf, len, "0x%"PRIx64, lip->data8[i]);
      buf += num;
      len -= num;
    }
  }
}
