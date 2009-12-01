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
#include <hpcrun/hpcrun_stats.h>
#include <hpcrun/thread_data.h>

#include <hpcrun/epoch.h>
#include <monitor.h>
#include <hpcrun/sample_event.h>

#include <messages/messages.h>
#include <lush/lush-backtrace.h>
#include <trampoline/common/trampoline.h>
#include <memory/hpcrun-malloc.h>

//***************************************************************************
// local constants & macros
//***************************************************************************

//***************************************************************************
// forward declarations 
//***************************************************************************

static cct_node_t*
_hpcrun_backtrace2cct(epoch_t* epoch, ucontext_t* context,
		      int metricId, uint64_t metricIncr,
		      int skipInner);

static bool _std_backtrace(backtrace_t* bt, ucontext_t* context);


//***************************************************************************
// interface functions
//***************************************************************************

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
hpcrun_backtrace2cct(epoch_t* epoch, ucontext_t* context, 
		     int metricId,
		     uint64_t metricIncr,
		     int skipInner, int isSync)
{
  cct_node_t* n = NULL;
  if (hpcrun_isLogicalUnwind()) {
    TMSG(LUSH,"lush backtrace2cct invoked");
    n = lush_backtrace2cct(epoch, context, metricId, metricIncr, skipInner,
			   isSync);
  }
  else {
    TMSG(LUSH,"regular (NON-lush) backtrace2cct invoked");
    n = _hpcrun_backtrace2cct(epoch, context, metricId, metricIncr, skipInner);
  }

  // N.B.: for lush_backtrace() it may be that n = NULL

  return n;
}

void 
dump_backtrace(epoch_t* epoch, frame_t* unwind)
{
  static const int msg_limit = 100;
  int msg_cnt = 0;

  char as_str[LUSH_ASSOC_INFO_STR_MIN_LEN];
  char lip_str[LUSH_LIP_STR_MIN_LEN];

  PMSG_LIMIT(EMSG("-- begin new backtrace (innermost first) ------------"));

  thread_data_t* td = hpcrun_get_thread_data();
  if (unwind) {
    for (frame_t* x = td->btbuf; x < unwind; ++x) {
      lush_assoc_info_sprintf(as_str, x->as_info);
      lush_lip_sprintf(lip_str, x->lip);
      PMSG_LIMIT(EMSG("%s: ip %p | lip %s", as_str, x->ip, lip_str));

      msg_cnt++;
      if (msg_cnt > msg_limit) {
        PMSG_LIMIT(EMSG("!!! message limit !!!"));
        break;
      }
    }
  }

  if (msg_cnt <= msg_limit && td->bufstk != td->bufend) {
    PMSG_LIMIT(EMSG("-- begin cached backtrace ---------------------------"));
    for (frame_t* x = td->bufstk; x < td->bufend; ++x) {
      lush_assoc_info_sprintf(as_str, x->as_info);
      lush_lip_sprintf(lip_str, x->lip);
      PMSG_LIMIT(EMSG("%s: ip %p | lip %s", as_str, x->ip, lip_str));

      msg_cnt++;
      if (msg_cnt > msg_limit) {
        PMSG_LIMIT(EMSG("!!! message limit !!!"));
        break;
      }
    }
  }

  PMSG_LIMIT(EMSG("-- end backtrace ------------------------------------\n"));
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
  if (! (bt->size - bt->len)) {
    return NULL; // FIX THIS
  }
  *(bt->cur) = *frame;
  frame_t* rv = bt->cur;
  bt->cur++;
  bt->len++;
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


bool
hpcrun_backtrace_std(backtrace_t* bt, ucontext_t* context)
{
  if (hpcrun_isLogicalUnwind()) {
    TMSG(LUSH,"lush backtrace invoked");
#ifdef OLD_BT
    n = lush_backtrace2cct(epoch, context, metricId, metricIncr, skipInner,
			   isSync);
#endif
    return true;
  }
  else {
    TMSG(LUSH,"regular (NON-lush) backtrace2cct invoked");
    // cct_node_t* n = _std_backtrace(bt, context);
    return true;
  }

  // N.B.: for lush_backtrace() it may be that n = NULL

  // return n;
}

//***************************************************************************
// private operations 
//***************************************************************************

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
static int
hpcrun_filter_sample(int len, frame_t *start, frame_t *last)
{
  return ( !(monitor_in_start_func_narrow(last->ip) && (len > 1)) );
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


static cct_node_t*
_hpcrun_backtrace2cct(epoch_t* epoch, ucontext_t* context,
		      int metricId,
		      uint64_t metricIncr,
		      int skipInner)
{
  int  backtrace_trolled = 0;
  bool tramp_found       = false;

  unw_cursor_t cursor;
  unw_init_cursor(&cursor, context);

  //--------------------------------------------------------------------
  // note: these variables are not local variables so that if a SIGSEGV 
  // occurs and control returns up several procedure frames, the values 
  // are accessible to a dumping routine that will tell us where we ran 
  // into a problem.
  //--------------------------------------------------------------------

  thread_data_t* td = hpcrun_get_thread_data();
  td->unwind   = td->btbuf; // innermost
  td->bufstk   = td->bufend;

  int unw_len = 0;
  while (true) {
    int ret;

    unw_word_t ip = 0;
    unw_get_reg(&cursor, UNW_REG_IP, &ip);

    if (hpcrun_trampoline_interior(ip)) {
      // bail; we shouldn't be unwinding here. hpcrun is in the midst of 
      // counting a return from a sampled frame using a trampoline.
      // drop the sample. 
      // FIXME: sharpen the information to indicate why the sample is 
      //        being dropped.
      unw_throw();
    }

    if (hpcrun_trampoline_at_entry(ip)) {
      if (unw_len == 0){
	// we are about to enter the trampoline code to synchronously 
	// record a return. for now, simply do nothing ...
	// FIXME: with a bit more effort, we could charge 
	//        the sample to the return address in the caller. 
	unw_throw();
      } else {
	// we have encountered a trampoline in the middle of an unwind.
	tramp_found = true;

	// no need to unwind further. the outer frames are already known.
	break;
      }
    }
    
    hpcrun_ensure_btbuf_avail();

    td->unwind->cursor = cursor;
    td->unwind->ip     = (void *) ip;
    td->unwind->ra_loc = NULL;
    frame_t* prev = td->unwind;
    td->unwind++;
    unw_len++;

    ret = unw_step(&cursor);
    backtrace_trolled = (ret == STEP_TROLL);
    if (ret <= 0) {
      break;
    }
    prev->ra_loc = hpcrun_unw_get_ra_loc(&cursor);
  }

  if (backtrace_trolled){
    hpcrun_up_pmsg_count();
  }
  
  frame_t* bt_beg  = td->btbuf;      // innermost, inclusive
  frame_t* bt_last = td->unwind - 1; // outermost, inclusive
  frame_t* bt_end  = bt_last + 1;    // outermost, exclusive
  size_t new_frame_count = bt_end - bt_beg;

  cct_node_t* cct_cursor = NULL;

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

    // start insertion below caller's frame, which is marked with the trampoline
    cct_cursor   = td->tramp_cct_node->parent;
  }
  else {
    hpcrun_cached_bt_adjust_size(new_frame_count);
    memmove(td->cached_bt, bt_beg, sizeof(frame_t) * new_frame_count);

    td->cached_bt_end = td->cached_bt + new_frame_count;
  }

  if (! ENABLED(NO_SAMPLE_FILTERING)) {
    frame_t* beg_frame  = td->cached_bt;
    frame_t* last_frame = td->cached_bt_end - 1;
    int num_frames      = last_frame - beg_frame + 1;
	
    if (hpcrun_filter_sample(num_frames, beg_frame, last_frame)){
      TMSG(SAMPLE_FILTER, "filter sample of length %d", num_frames);
      frame_t *fr = beg_frame;
      for (int i = 0; i < num_frames; i++, fr++){
	TMSG(SAMPLE_FILTER,"  frame ip[%d] = %p", i, fr->ip);
      }
      hpcrun_stats_num_samples_filtered_inc();
      return 0;
    }
  }

  //
  // FIXME: For the moment, ignore skipInner issues with trampolines.
  //        Eventually, this will need to be addressed
  //
  if (skipInner) {
    EMSG("WARNING: backtrace detects skipInner != 0 (skipInner = %d)", 
	 skipInner);
    bt_beg = hpcrun_skip_chords(bt_last, bt_beg, skipInner);
  }

  cct_node_t* n;
  n = hpcrun_cct_insert_backtrace(&(epoch->csdata), cct_cursor, metricId,
				  bt_last, bt_beg,
				  (cct_metric_data_t){.i = metricIncr});

  if (ENABLED(USE_TRAMP)){
    hpcrun_trampoline_remove();
    td->tramp_frame = td->cached_bt;
    hpcrun_trampoline_insert(n);
  }

  return n;
}

static bool
_std_backtrace(backtrace_t* bt, ucontext_t* context)
{
  int  backtrace_trolled = 0;
  bool tramp_found       = false;

  unw_cursor_t cursor;
  unw_init_cursor(&cursor, context);

  int unw_len = 0;
  while (true) {
    int ret;

    unw_word_t ip = 0;
    unw_get_reg(&cursor, UNW_REG_IP, &ip);

    if (hpcrun_trampoline_interior(ip)) {
      // bail; we shouldn't be unwinding here. hpcrun is in the midst of 
      // counting a return from a sampled frame using a trampoline.
      // drop the sample. 
      // FIXME: sharpen the information to indicate why the sample is 
      //        being dropped.
      unw_throw();
    }

    if (hpcrun_trampoline_at_entry(ip)) {
      if (unw_len == 0){
	// we are about to enter the trampoline code to synchronously 
	// record a return. for now, simply do nothing ...
	// FIXME: with a bit more effort, we could charge 
	//        the sample to the return address in the caller. 
	unw_throw();
      } else {
	// we have encountered a trampoline in the middle of an unwind.
	tramp_found = true;

	// no need to unwind further. the outer frames are already known.
	break;
      }
    }
    
    frame_t* prev = hpcrun_bt_push(bt,
				   &((frame_t){.cursor = cursor, .ip = (void*)ip, .ra_loc = NULL}));
    unw_len++;

    ret = unw_step(&cursor);
    backtrace_trolled = (ret == STEP_TROLL);
    if (ret <= 0) {
      break;
    }
    prev->ra_loc = hpcrun_unw_get_ra_loc(&cursor);
  }

  if (backtrace_trolled){
    hpcrun_up_pmsg_count();
  }
  
  frame_t* bt_beg  	 = hpcrun_bt_beg(bt);
  frame_t* bt_last 	 = hpcrun_bt_cur(bt);
  size_t new_frame_count = hpcrun_bt_len(bt);

  cct_node_t* cct_cursor = NULL;

  thread_data_t* td    = hpcrun_get_thread_data();

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

    // start insertion below caller's frame, which is marked with the trampoline
    cct_cursor   = td->tramp_cct_node->parent;
  }
  else {
    hpcrun_cached_bt_adjust_size(new_frame_count);
    memmove(td->cached_bt, bt_beg, sizeof(frame_t) * new_frame_count);

    td->cached_bt_end = td->cached_bt + new_frame_count;
  }

  if (! ENABLED(NO_SAMPLE_FILTERING)) {
    frame_t* beg_frame  = td->cached_bt;
    frame_t* last_frame = td->cached_bt_end - 1;
    int num_frames      = last_frame - beg_frame + 1;
	
    if (hpcrun_filter_sample(num_frames, beg_frame, last_frame)){
      TMSG(SAMPLE_FILTER, "filter sample of length %d", num_frames);
      frame_t *fr = beg_frame;
      for (int i = 0; i < num_frames; i++, fr++){
	TMSG(SAMPLE_FILTER,"  frame ip[%d] = %p", i, fr->ip);
      }
      hpcrun_stats_num_samples_filtered_inc();
      return false;
    }
  }

  return true;

#ifdef OLD_BT
  //
  // FIXME: For the moment, ignore skipInner issues with trampolines.
  //        Eventually, this will need to be addressed
  //
  if (skipInner) {
    EMSG("WARNING: backtrace detects skipInner != 0 (skipInner = %d)", 
	 skipInner);
    bt_beg = hpcrun_skip_chords(bt_last, bt_beg, skipInner);
  }

  cct_node_t* n;
  n = hpcrun_cct_insert_backtrace(&(epoch->csdata), cct_cursor, metricId,
				  bt_last, bt_beg,
				  (cct_metric_data_t){.i = metricIncr});

  if (ENABLED(USE_TRAMP)){
    hpcrun_trampoline_remove();
    td->tramp_frame = td->cached_bt;
    hpcrun_trampoline_insert(n);
  }

  return n;
#endif
}
