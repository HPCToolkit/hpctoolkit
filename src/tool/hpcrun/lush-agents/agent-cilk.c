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
//
// File: 
//   $HeadURL$
//
// Purpose:
//   LUSH: Logical Unwind Support for HPCToolkit
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
// Author:
//   Nathan Tallent, Rice University.
//
//***************************************************************************

//************************* System Include Files ****************************

#include <stdlib.h>
#include <stdbool.h>

#include <pthread.h>

#include <string.h>

#include <limits.h>  // PATH_MAX
#include <errno.h>

//*************************** User Include Files ****************************

#include "agent-cilk.h"

#include <messages/messages.h>

//*************************** Forward Declarations **************************

#define LUSHCB_DECL(FN) \
 LUSH ## FN ## _fn_t  FN

LUSHCB_DECL(CB_malloc);
LUSHCB_DECL(CB_free);
LUSHCB_DECL(CB_step);
LUSHCB_DECL(CB_loadmap_find);
// lush_cursor stuff

#undef LUSHCB_DECL

static lush_agentid_t MY_lush_aid;

//*************************** Forward Declarations **************************

// FIXME: hackish implementation
static const char* libcilk_str = "libcilk";
static const char* lib_str = "lib";
static const char* ld_str = "ld-linux";

//*************************** Forward Declarations **************************

static int
init_lcursor(lush_cursor_t* cursor);

static unw_seg_t
classify_by_unw_segment(cilk_cursor_t* csr);

static unw_seg_t
peek_segment(lush_cursor_t* cursor);

static bool
is_libcilk(void* addr, char* lm_buffer /*helper storage*/);

static bool
is_cilkprogram(void* addr, char* lm_buffer /*helper storage*/);


// **************************************************************************
// Initialization/Finalization
// **************************************************************************

extern int
LUSHI_init(int argc, char** argv,
	   lush_agentid_t           aid,
	   LUSHCB_malloc_fn_t       malloc_fn,
	   LUSHCB_free_fn_t         free_fn,
	   LUSHCB_step_fn_t         step_fn,
	   LUSHCB_loadmap_find_fn_t loadmap_fn)
{
  MY_lush_aid = aid;

  CB_malloc       = malloc_fn;
  CB_free         = free_fn;
  CB_step         = step_fn;
  CB_loadmap_find = loadmap_fn;

  return 0;
}


extern int 
LUSHI_fini()
{
  return 0;
}


extern char* 
LUSHI_strerror(int code)
{
  return ""; // STUB
}


// **************************************************************************
// Maintaining Responsibility for Code/Frame-space
// **************************************************************************

extern int 
LUSHI_reg_dlopen()
{
  return 0; // FIXME: coordinate with dylib stuff
}


extern bool 
LUSHI_ismycode(void* addr)
{
  char buffer[PATH_MAX];
  return (is_libcilk(addr, buffer) || is_cilkprogram(addr, buffer));
}


bool
is_libcilk(void* addr, char* lm_name /*helper storage*/)
{
  void *lm_beg, *lm_end;
  int ret = CB_loadmap_find(addr, lm_name, &lm_beg, &lm_end);
  if (ret && strstr(lm_name, libcilk_str)) {
    return true;
  }
  return false;
}


bool
is_cilkprogram(void* addr, char* lm_name /*helper storage*/)
{
  void *lm_beg, *lm_end;
  int ret = CB_loadmap_find(addr, lm_name, &lm_beg, &lm_end); 
  if (ret && !(strstr(lm_name, lib_str) || strstr(lm_name, ld_str))) {
    return true;
  }
  return false;
}


// **************************************************************************
// 
// **************************************************************************

extern lush_step_t
LUSHI_step_bichord(lush_cursor_t* cursor)
{
  // -------------------------------------------------------
  // Initialize cursor
  // -------------------------------------------------------
  int ret = init_lcursor(cursor);
  if (ret != 0) {
    return LUSH_STEP_ERROR;
  }
  cilk_cursor_t* csr = (cilk_cursor_t*)lush_cursor_get_lcursor(cursor);

  // -------------------------------------------------------
  // Compute p-note's current stack segment ('cur_seg')
  // -------------------------------------------------------
  unw_seg_t cur_seg = classify_by_unw_segment(csr);

  // -------------------------------------------------------
  // Given p-note derive l-note:
  // -------------------------------------------------------

  // FIXME: consider effects of multiple agents
  //lush_agentid_t last_aid = lush_cursor_get_aid(cursor); 

  if (cur_seg == UnwSeg_CilkRT) {
    // INVARIANT: unw_ty_is_worker() == true
    lush_cursor_set_assoc(cursor, LUSH_ASSOC_1_to_0);
  }
  else if (cur_seg == UnwSeg_User) {
    // INVARIANT: unw_ty_is_worker() == true
    CilkWorkerState* ws = csr->u.cilk_worker_state;
    if (ws && ws->self == 0 && peek_segment(cursor) == UnwSeg_CilkSched) {
      // local stack: import_cilk_main -> cilk_main (skip the former)
      lush_cursor_set_assoc(cursor, LUSH_ASSOC_0_to_0); // skip
    }
    else {
      lush_cursor_set_assoc(cursor, LUSH_ASSOC_1_to_1);
    }
  }
  else if (cur_seg == UnwSeg_CilkSched) {
    switch (csr->u.ty) {
      case UnwTy_Master:
	lush_cursor_set_assoc(cursor, LUSH_ASSOC_1_to_0);
	break;
      case UnwTy_WorkerLcl:
	if (csr->u.prev_seg == UnwSeg_User 
	    && !csr_is_flag(csr, UnwFlg_HaveLCtxt)) {
	  lush_cursor_set_assoc(cursor, LUSH_ASSOC_1_to_1);
	  csr_set_flag(csr, UnwFlg_HaveLCtxt);
    	}
	else if (csr_is_flag(csr, UnwFlg_HaveLCtxt)) {
	  lush_cursor_set_assoc(cursor, LUSH_ASSOC_0_to_0); // skip
	}
	else {
	  lush_cursor_set_assoc(cursor, LUSH_ASSOC_1_to_0);
	}
	break;
      case UnwTy_Worker:
	if (csr->u.prev_seg == UnwSeg_User 
	    && !csr_is_flag(csr, UnwFlg_HaveLCtxt)) {
	  lush_cursor_set_assoc(cursor, LUSH_ASSOC_1_to_M);
	  csr_set_flag(csr, UnwFlg_HaveLCtxt);
	}
	else if (csr_is_flag(csr, UnwFlg_HaveLCtxt)) {
	  lush_cursor_set_assoc(cursor, LUSH_ASSOC_0_to_0); // skip
	}
	else {
	  lush_cursor_set_assoc(cursor, LUSH_ASSOC_1_to_0);
	}
	break;
      default: EEMSG("FIXME: default case (assert)\n");
    }
  }
  else {
    EEMSG("FIXME: Unknown segment! (assert)\n");
  }


  // record for next bi-chord
  csr->u.prev_seg = cur_seg;

  return LUSH_STEP_CONT;
}


extern lush_step_t
LUSHI_step_pnote(lush_cursor_t* cursor)
{
  // NOTE: Since all associations are 1-to-a, it is always valid to step.

  lush_step_t ty = LUSH_STEP_NULL;
  
  int t = CB_step(lush_cursor_get_pcursor(cursor));
  if (t > 0) {
    ty = LUSH_STEP_END_CHORD;
  }
  else if (t == 0) {
    ty = LUSH_STEP_END_PROJ;
  }
  else /* (t < 0) */ {
    ty = LUSH_STEP_ERROR;
  }
  
  return ty;
}


#define SET_LIP_AND_TY(cl, lip, ty)					\
  if (!cl) {								\
    cilk_ip_set(lip, ip_normalized_NULL_lval);				\
    ty = LUSH_STEP_END_CHORD;						\
  }									\
  else {								\
    /* NOTE: interior lips should act like a return address; */		\
    /*   therefore, we add 1                                 */		\
    ip_normalized_t ip =						\
      hpcrun_normalize_ip(CILKFRM_PROC(cl->frame) + 1, NULL);		\
    cilk_ip_set(lip, ip);						\
    ty = LUSH_STEP_CONT;						\
  }

extern lush_step_t
LUSHI_step_lnote(lush_cursor_t* cursor)
{
  lush_step_t ty = LUSH_STEP_NULL;

  lush_assoc_t as = lush_cursor_get_assoc(cursor);
  cilk_cursor_t* csr = (cilk_cursor_t*)lush_cursor_get_lcursor(cursor);
  cilk_ip_t* lip = (cilk_ip_t*)lush_cursor_get_lip(cursor);
  
  if (lush_assoc_is_a_to_0(as)) {
    ty = LUSH_STEP_END_CHORD;
  }
  else if (as == LUSH_ASSOC_1_to_1) {
    if (csr_is_flag(csr, UnwFlg_BegLNote)) {
      ty = LUSH_STEP_END_CHORD;
    }
    else {
      cilk_ip_set(lip, csr->u.ref_ip_norm);
      ty = LUSH_STEP_CONT;
      csr_set_flag(csr, UnwFlg_BegLNote);
    }
  }
  else if (as == LUSH_ASSOC_1_to_M) {
    // INVARIANT: csr->u.cilk_closure is non-NULL
    Closure* cl = csr->u.cilk_closure;
    if (csr_is_flag(csr, UnwFlg_BegLNote)) { // after innermost closure
      // advance to next closure
      cl = csr->u.cilk_closure = cl->parent;
      SET_LIP_AND_TY(cl, lip, ty);
    }
    else {
      // skip innermost closure; it is identical to the outermost stack frame
      cl = csr->u.cilk_closure = cl->parent;
      SET_LIP_AND_TY(cl, lip, ty);
      csr_set_flag(csr, UnwFlg_BegLNote);
    }
  }
  else {
    ty = LUSH_STEP_ERROR;
  }

  return ty;
}


extern int 
LUSHI_set_active_frame_marker(/*ctxt, cb*/)
{
  return 0; // STUB
}


// **************************************************************************
// 
// **************************************************************************

static int
init_lcursor(lush_cursor_t* cursor)
{
  lush_lip_t* lip = lush_cursor_get_lip(cursor);
  cilk_cursor_t* csr = (cilk_cursor_t*)lush_cursor_get_lcursor(cursor);
  lush_agentid_t aid_prev = lush_cursor_get_aid_prev(cursor);
  
  // -------------------------------------------------------
  // inter-bichord data
  // -------------------------------------------------------
  if (aid_prev == lush_agentid_NULL) {
    CilkWorkerState* ws = 
      (CilkWorkerState*)pthread_getspecific(CILK_WorkerState_key);
    Closure* cactus_stack = (ws) ? CILKWS_CL_DEQ_BOT(ws) : NULL;

    // unw_ty_t
    if (!ws) {
      csr->u.ty = UnwTy_Master;
    }
    else {
      csr->u.ty = (cactus_stack) ? UnwTy_Worker : UnwTy_WorkerLcl;

      if (CILK_WS_has_stolen(ws) && !cactus_stack) {
	// Sometimes we are hammered in the middle of a Cilk operation
	// where the cactus stack pointer becomes NULL, even though
	// logical context 'should' exist (e.g., Cilk_sync before a
	// return).  The result is that we cannot obtain the logical
	// context and therefore cannot locate the unwind correctly
	// within the CCT.
	//
	// To avoid such bad unwinds, we use the simple heuristic of
	// requiring a cactus stack pointer if the worker has become a
	// thief. It is not a perfect solution since a worker may have
	// stolen the main routine; however, this is an exceptional
	// case.
	//
	// FIXME: the best solution is to find the smallest window
	// within the scheduler code and set a flag.  But I found that
	// this is easier said than done.
	return 1;
      }
    }

    csr->u.prev_seg = UnwSeg_NULL;

    csr->u.flg = UnwFlg_NULL;

    csr->u.cilk_worker_state = ws;
    csr->u.cilk_closure = cactus_stack;
  }
  else if (aid_prev != MY_lush_aid) {
    EEMSG("FIXME: HOW TO HANDLE MULTIPLE AGENTS?\n");
  }

  // -------------------------------------------------------
  // intra-bichord data
  // -------------------------------------------------------
  memset(lip, 0, sizeof(*lip));

  csr->u.ref_ip = (void*)lush_cursor_get_ip_unnorm(cursor);
  csr->u.ref_ip_norm = lush_cursor_get_ip_norm(cursor);

  csr_unset_flag(csr, UnwFlg_BegLNote);

  return 0;
}


// Classify the cursor by the unw segment (physical stack)
// NOTE: may set a flag within 'csr'
static unw_seg_t
classify_by_unw_segment(cilk_cursor_t* csr)
{
  unw_seg_t cur_seg = UnwSeg_NULL;

  char buffer[PATH_MAX];
  bool is_cilkrt = is_libcilk(csr->u.ref_ip, buffer);
  bool is_user   = is_cilkprogram(csr->u.ref_ip, buffer);

  if (unw_ty_is_worker(csr->u.ty)) {
    // -------------------------------------------------------
    // 1. is_cilkrt &  (deq_diff <= 1) => UnwSeg_CilkSched 
    // 2. is_cilkrt & !(deq_diff <= 1) => UnwSeg_CilkRT
    // 3. is_user                      => UnwSeg_User
    // -------------------------------------------------------
    CilkWorkerState* ws = csr->u.cilk_worker_state;
    long deq_diff = CILKWS_FRAME_DEQ_TAIL(ws) - CILKWS_FRAME_DEQ_HEAD(ws);

    if (is_user) {
      cur_seg = UnwSeg_User;
      csr_set_flag(csr, UnwFlg_SeenUser);
    }
    else if (is_cilkrt) {
      cur_seg = (deq_diff <= 1) ? UnwSeg_CilkSched : UnwSeg_CilkRT;

      // FIXME: sometimes the above test is not correct... OVERRIDE
      if (cur_seg == UnwSeg_CilkRT && csr_is_flag(csr, UnwFlg_SeenUser)) {
	cur_seg = UnwSeg_CilkSched;
      }
    }
    else {
      EEMSG("FIXME: (assert): neither cilkrt nor user\n");
    }
  }
  else if (unw_ty_is_master(csr->u.ty)) {
    cur_seg = UnwSeg_CilkSched;
    if ( !(is_user || is_cilkrt) ) {
      // is_user may be true when executing main
      EEMSG("FIXME: Unknown segment for master (assert)\n"); 
    }
  }
  else {
    EEMSG("FIXME: Unknown thread type! (assert)\n");
  }

  return cur_seg;
}


static unw_seg_t
peek_segment(lush_cursor_t* cursor)
{
  lush_cursor_t tmp_cursor = *cursor;

  unw_seg_t cur_seg = UnwSeg_NULL;

  lush_step_t ty = LUSHI_step_pnote(&tmp_cursor);
  if ( !(ty == LUSH_STEP_END_PROJ || ty == LUSH_STEP_ERROR) ) {
    cilk_cursor_t* csr = (cilk_cursor_t*)lush_cursor_get_lcursor(&tmp_cursor);
    csr->u.ref_ip = (void*)lush_cursor_get_ip_unnorm(&tmp_cursor);
    csr->u.ref_ip_norm = lush_cursor_get_ip_norm(&tmp_cursor);
    cur_seg = classify_by_unw_segment(csr);
  }

  return cur_seg;
}


// **************************************************************************
// 
// **************************************************************************

extern int
LUSHI_lip_destroy(lush_lip_t* lip)
{
  return 0; // STUB
}


extern int 
LUSHI_lip_eq(lush_lip_t* lip)
{
  return 0; // STUB
}


extern int
LUSHI_lip_read()
{
  return 0; // STUB
}


extern int
LUSHI_lip_write()
{
  return 0; // STUB
}


// **************************************************************************
// Metrics
// **************************************************************************

extern bool
LUSHI_do_metric(uint64_t incrMetricIn, 
		bool* doMetric, bool* doMetricIdleness, 
		uint64_t* incrMetric, double* incrMetricIdleness)
{
  // INVARIANT: at least one thread is working
  // INVARIANT: ws is non-NULL

  CilkWorkerState* ws = 
    (CilkWorkerState*)pthread_getspecific(CILK_WorkerState_key);
  bool isWorking = (ws && CILK_WS_is_working(ws));

  if (isWorking) {
    double n = CILK_WS_num_workers(ws);
    double n_working = (double)CILK_Threads_Working;
    double n_not_working = n - n_working;

    // NOTE: if n_working == 0, then Cilk should be in the process of
    // exiting.  Protect against samples in this timing window.
    double idleness = 0.0;
    if (n_working > 0) {
      idleness = ((double)incrMetricIn * n_not_working) / n_working;
    }

    *doMetric = true;
    *doMetricIdleness = true;
    *incrMetric = incrMetricIn;
    *incrMetricIdleness = idleness;
  }
  else {
    *doMetric = false;
    *doMetricIdleness = false;
    //*incrMetric = 0;
    //*incrMetricIdleness = 0.0;
  }
  return *doMetric;
}
 

// **************************************************************************
