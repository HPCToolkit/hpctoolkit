// -*-Mode: C++;-*- // technically C99
// $Id$

//***************************************************************************
//
// File: 
//    $Source$
//
// Purpose:
//    LUSH: Logical Unwind Support for HPCToolkit
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
// Author:
//    Nathan Tallent, Rice University.
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

#include <general.h> // FIXME: for MSG -- but should not include
#include <pmsg.h>

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

static void
init_lcursor(lush_cursor_t* cursor);

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
  init_lcursor(cursor);
  cilk_cursor_t* csr = (cilk_cursor_t*)lush_cursor_get_lcursor(cursor);

  // -------------------------------------------------------
  // Compute p-note's current stack segment ('cur_seg')
  // -------------------------------------------------------
  unw_seg_t cur_seg = UNW_SEG_NULL;

  char buffer[PATH_MAX];
  bool is_cilkrt = is_libcilk(csr->u.ref_ip, buffer);
  bool is_user   = is_cilkprogram(csr->u.ref_ip, buffer);

  if (unw_ty_is_worker(csr->u.ty)) {
    // -------------------------------------------------------
    // 1. is_cilkrt &  (deq_diff <= 1) => UNW_SEG_CILKSCHED 
    // 2. is_cilkrt & !(deq_diff <= 1) => UNW_SEG_CILKRT
    // 3. is_user                      => UNW_SEG_USER
    // -------------------------------------------------------
    CilkWorkerState* ws = csr->u.cilk_worker_state;
    long deq_diff = CILKWS_FRAME_DEQ_TAIL(ws) - CILKWS_FRAME_DEQ_HEAD(ws);

    if (is_user) {
      cur_seg = UNW_SEG_USER;
      csr_set_flag(csr, UNW_FLG_SEEN_USER);
    }
    else if (is_cilkrt) {
      cur_seg = (deq_diff <= 1) ? UNW_SEG_CILKSCHED : UNW_SEG_CILKRT;

      // FIXME: sometimes the above test is not correct... OVERRIDE
      if (cur_seg == UNW_SEG_CILKRT && csr_is_flag(csr, UNW_FLG_SEEN_USER)) {
	cur_seg = UNW_SEG_CILKSCHED;
      }
    }
    else {
      fprintf(stderr, "FIXME: (assert): neither cilkrt nor user\n");
    }
  }
  else if (unw_ty_is_master(csr->u.ty)) {
    cur_seg = UNW_SEG_CILKSCHED;
    if ( !(is_user || is_cilkrt) ) {
      // is_user may be true when executing main
      fprintf(stderr, "FIXME: Unknown segment for master (assert)\n"); 
    }
  }
  else {
    fprintf(stderr, "FIXME: Unknown thread type! (assert)\n");
  }

  // -------------------------------------------------------
  // Given p-note derive l-note:
  // -------------------------------------------------------

  // FIXME: consider effects of multiple agents
  //lush_agentid_t last_aid = lush_cursor_get_aid(cursor); 

  if (cur_seg == UNW_SEG_CILKRT) {
    // INVARIANT: unw_ty_is_worker() == true
    lush_cursor_set_assoc(cursor, LUSH_ASSOC_1_to_0);
  }
  else if (cur_seg == UNW_SEG_USER) {
    // INVARIANT: unw_ty_is_worker() == true
    lush_cursor_set_assoc(cursor, LUSH_ASSOC_1_to_1);
  }
  else if (cur_seg == UNW_SEG_CILKSCHED) {
    switch (csr->u.ty) {
      case UnwTy_Master:
	lush_cursor_set_assoc(cursor, LUSH_ASSOC_1_to_0);
	break;
      case UnwTy_WorkerLcl:
	if (csr->u.prev_seg == UNW_SEG_USER 
	    && !csr_is_flag(csr, UNW_FLG_HAVE_LCTXT)) {
	  lush_cursor_set_assoc(cursor, LUSH_ASSOC_1_to_1);
	  csr_set_flag(csr, UNW_FLG_HAVE_LCTXT);
    	}
	else if (csr_is_flag(csr, UNW_FLG_HAVE_LCTXT)) {
	  lush_cursor_set_assoc(cursor, LUSH_ASSOC_0_to_0);
	}
	else {
	  lush_cursor_set_assoc(cursor, LUSH_ASSOC_1_to_0);
	}
	break;
      case UnwTy_Worker:
	if (csr->u.prev_seg == UNW_SEG_USER 
	    && !csr_is_flag(csr, UNW_FLG_HAVE_LCTXT)) {
	  lush_cursor_set_assoc(cursor, LUSH_ASSOC_1_to_M);
	  csr_set_flag(csr, UNW_FLG_HAVE_LCTXT);
	}
	else if (csr_is_flag(csr, UNW_FLG_HAVE_LCTXT)) {
	  lush_cursor_set_assoc(cursor, LUSH_ASSOC_1_to_0);
	}
	else {
	  lush_cursor_set_assoc(cursor, LUSH_ASSOC_1_to_0);
	}
	break;
      default: fprintf(stderr, "FIXME: default case (assert)\n");
    }
  }
  else {
    fprintf(stderr, "FIXME: Unknown segment! (assert)\n");
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


#define SET_LIP_AND_TY(cl, lip, ty)				\
  if (!cl) {							\
    cilk_ip_init(lip, NULL /*0*/);				\
    ty = LUSH_STEP_END_CHORD;					\
  }								\
  else {							\
    cilk_ip_init(lip, CILKFRM_PROC(cl->frame) /*cl->status*/);	\
    ty = LUSH_STEP_CONT;					\
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
    if (csr_is_flag(csr, UNW_FLG_BEG_LNOTE)) {
      ty = LUSH_STEP_END_CHORD;
    }
    else {
      cilk_ip_init(lip, csr->u.ref_ip /*0*/);
      ty = LUSH_STEP_CONT;
      csr_set_flag(csr, UNW_FLG_BEG_LNOTE);
    }
  }
  else if (as == LUSH_ASSOC_1_to_M) {
    // INVARIANT: csr->u.cilk_closure is non-NULL
    Closure* cl = csr->u.cilk_closure;
    if (csr_is_flag(csr, UNW_FLG_BEG_LNOTE)) { // past begining of lchord
      // advance to next closure
      cl = csr->u.cilk_closure = cl->parent;
      SET_LIP_AND_TY(cl, lip, ty);
    }
    else {
      // skip the top closure; it is identical to the outermost stack frame
      cl = csr->u.cilk_closure = cl->parent;
      SET_LIP_AND_TY(cl, lip, ty);
      csr_set_flag(csr, UNW_FLG_BEG_LNOTE);
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


// --------------------------------------------------------------------------

void
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
    }

    csr->u.prev_seg = UNW_SEG_NULL;

    csr->u.flg = UNW_FLG_NULL;

    csr->u.cilk_worker_state = ws;
    csr->u.cilk_closure = cactus_stack;
  }
  else if (aid_prev != MY_lush_aid) {
    fprintf(stderr, "FIXME: HOW TO HANDLE MULTIPLE AGENTS?\n");
  }

  // -------------------------------------------------------
  // intra-bichord data
  // -------------------------------------------------------
  memset(lip, 0, sizeof(*lip));

  csr->u.ref_ip = (void*)lush_cursor_get_ip(cursor);
  csr_unset_flag(csr, UNW_FLG_BEG_LNOTE);
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
// Concurrency
// **************************************************************************

extern int
LUSHI_do_backtrace()
{
  // Do the unwind if this is a working thread
  CilkWorkerState* ws = 
    (CilkWorkerState*)pthread_getspecific(CILK_WorkerState_key);
  
  return (ws && CILK_WS_is_working(ws));
}

extern double
LUSHI_get_idleness()
{
  // INVARIANT: at least one thread is working
  // INVARIANT: ws is non-NULL

  CilkWorkerState* ws = 
    (CilkWorkerState*)pthread_getspecific(CILK_WorkerState_key);

  double n = CILK_WS_num_workers(ws);
  double n_working = (double)CILK_Threads_Working;
  double n_not_working = n - n_working;

  double idleness = 0.0;

  // NOTE: if n_working == 0, then Cilk should be in the process of
  // exiting.  Protect against samples in this timing window.
  if (n_working > 0) {
    idleness = n_not_working / n_working;
  }
  return idleness;
}

// **************************************************************************
