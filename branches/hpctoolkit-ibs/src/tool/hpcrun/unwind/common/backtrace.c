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
// Copyright ((c)) 2002-2011, Rice University
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

#include <messages/messages.h>
#include <lush/lush-backtrace.h>
#include <trampoline/common/trampoline.h>
#include <memory/hpcrun-malloc.h>
#include <dbg_backtrace.h>

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

//
// FIXME: upgrade to lush
//
void
hpcrun_bt_dump(backtrace_t* bt, const char* tag)
{
  const char* mytag = (tag) ? tag : "";

  char as_str[LUSH_ASSOC_INFO_STR_MIN_LEN];
  char lip_str[LUSH_LIP_STR_MIN_LEN];

  EMSG("-- begin new backtrace (innermost first) [%s] ----------", mytag);
  for (frame_t* f = bt->beg; f < bt->cur; f++) {
    lush_assoc_info2str(as_str, sizeof(as_str), f->as_info);
    lush_lip2str(lip_str, sizeof(lip_str), f->lip);

    unw_word_t ip;
    hpcrun_unw_get_ip_unnorm_reg(&(f->cursor), &ip);

    load_module_t* lm = hpcrun_loadmap_findById(f->ip_norm.lm_id);
    const char* lm_name = (lm) ? lm->name : "(null)";

    EMSG("%s: ip = %p (%p), load module = %s | lip %s", as_str,
	 ip, f->ip_norm.lm_ip, lm_name, lip_str);
  }
  EMSG("-- end new backtrace                   [%s] ----------", mytag);
  if (bt->tramp)
    EMSG("-- (backtrace abbreviated by trampoline)    ----------", mytag);
  // FIXME? track rest of unwind via tramp node?
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
#if 0
bool
hpcrun_generate_backtrace(backtrace_info_t* bt,
			  ucontext_t* context, int skipInner)
{
  bt->has_tramp = false;
  bt->trolled  = false;
  bt->n_trolls = 0;

  bool tramp_found = false;

  step_state ret = STEP_ERROR; // default return value from stepper

  hpcrun_unw_cursor_t cursor;
  hpcrun_unw_init_cursor(&cursor, context);

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
    unw_word_t ip;
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

    ret = hpcrun_unw_step(&cursor);
    if (ret == STEP_TROLL) {
      bt->trolled = true;
      bt->n_trolls++;
    }
    if (ret <= 0) {
      if (ret == STEP_ERROR) {
	hpcrun_stats_num_samples_dropped_inc();
      }
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

#if 0 // no more filtering samples
  if (! ENABLED(NO_SAMPLE_FILTERING)) {
    frame_t* beg_frame  = td->cached_bt;
    frame_t* last_frame = td->cached_bt_end - 1;
    int num_frames      = last_frame - beg_frame + 1;
	
    if (hpcrun_filter_sample(num_frames, beg_frame, last_frame)){
      TMSG(PARTIAL_UNW, "PRE_FILTER frame count = %d", (*retn_bt_end - *retn_bt_beg) + 1);
      TMSG(SAMPLE_FILTER, "filter sample of length %d", num_frames);
      frame_t *fr = beg_frame;
      for (int i = 0; i < num_frames; i++, fr++){
	TMSG(SAMPLE_FILTER,"  frame ip[%d] ==> lm_id = %d and lm_ip = %p", i, fr->ip_norm.lm_id, fr->ip_norm.lm_ip);
      }
      hpcrun_stats_num_samples_filtered_inc();

      return false; // filtered sample ==> no cct entry
    }
  }
#endif // no filtering samples

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

  return true;
}
#endif

static void
skip_inner_fn(backtrace_t* bt, void* skip)
{
  size_t iskip = (size_t) skip;
  if (bt->tramp) return; // no skip inner if trampoline involved
  for (int i = 0; i < iskip && (! hpcrun_bt_empty(bt)); i++) {
    hpcrun_bt_pull_inner(bt);
  }
}

//
// implement using new backtrace data structure
//
bool
hpcrun_generate_backtrace(backtrace_info_t* bt_i,
			  ucontext_t* context, int skipInner)
{
  uintptr_t arg = (uintptr_t) skipInner;
  bool ret = hpcrun_gen_bt(context, skip_inner_fn, (bt_fn_arg) arg);
  backtrace_t* bt = &(TD_GET(bt));
  bt_i->begin = bt->beg;
  bt_i->last  = bt->cur - 1;
  bt_i->has_tramp = bt->tramp;
  bt_i->trolled   = bt->troll;
  bt_i->n_trolls  = bt->n_trolls;
  return ret;
}

// debug variant of above routine
//
//
// Generate a backtrace, store it in the thread local data
// Return true/false success code
// Also, return (via reference params) backtrace beginning, backtrace end,
//  and whether or not a trampoline was found.
//

#ifdef OLD_BT_BUF
bool
hpcrun_dbg_generate_backtrace(backtrace_info_t* bt,
			      ucontext_t* context, int skipInner)
{
  bt->has_tramp = false;
  bt->trolled  = false;
  bt->n_trolls = 0;

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
    unw_word_t ip;
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

  return true;
}
#endif // OLD_BT_BUF


//
// New operations
//

frame_t*
hpcrun_bt_reset(backtrace_t* bt)
{
  bt->cur = bt->beg;
  bt->len = 0;
  bt->tramp = false;
  bt->troll = false;
  bt->n_trolls = 0;
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
  bt->cur++;
  bt->len++;
  TMSG(BT, "after push, len(bt) = %d", bt->len);
  return bt->cur - 1;
}

frame_t*
hpcrun_bt_pull_inner(backtrace_t* bt)
{
  if (! bt->len) return NULL;
  bt->beg++;
  bt->len--;
  return bt->beg - 1;
}

frame_t*
hpcrun_bt_pull_outer(backtrace_t* bt)
{
  if (! bt->len) return NULL;
  bt->len--;
  return bt->beg + bt->len;
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
hpcrun_bt_tramp(backtrace_t* bt)
{
  return bt->tramp;
}

bool
hpcrun_bt_troll(backtrace_t* bt)
{
  return bt->troll;
}

int
hpcrun_bt_n_trolls(backtrace_t* bt)
{
  return bt->n_trolls;
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
  bt->len -= realskip;
}

void
hpcrun_dump_bt(backtrace_t* bt)
{
  for(frame_t* f = bt->beg; f < bt->beg + bt->len; f++) {
    TMSG(BT, "ip_norm.lm_id = %d, and ip_norm.lm_ip = %p ", f->ip_norm.lm_id,
	 f->ip_norm.lm_ip);
  }
}

//
// generate a backtrace, store it in thread-local data
// return success/failure
//

bool
hpcrun_gen_bt(ucontext_t* context,
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

    unw_word_t ip = 0;
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
    // FIXME: mfagan Probably don't need both n_trolls and troll flag
    if (ret == STEP_TROLL) {
      bt->troll = true;
      bt->n_trolls++;
      backtrace_trolled = true;
    }
    if (ret <= 0) {
      if (ret == STEP_ERROR)
	hpcrun_stats_num_samples_dropped_inc();
      break;
    }
    prev->ra_loc = hpcrun_unw_get_ra_loc(&cursor);
  }

  if (backtrace_trolled || (ret == STEP_ERROR)){
    hpcrun_up_pmsg_count();
  }
				    
  frame_t* bt_beg  = hpcrun_bt_beg(bt);
  size_t new_frame_count = hpcrun_bt_len(bt);

#if 0
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
  bt->tramp = tramp_found;
#endif

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
