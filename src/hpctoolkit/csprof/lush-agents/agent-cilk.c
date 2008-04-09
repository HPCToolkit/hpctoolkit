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
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <pthread.h>

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
LUSHCB_DECL(CB_get_loadmap);
// lush_cursor stuff

#undef LUSHCB_DECL

static lush_agentid_t MY_lush_aid;

//*************************** Forward Declarations **************************

// FIXME: hackish implementation
static const char* libcilk_str = "libcilk";
static const char* lib_str = "lib";
static const char* ld_str = "ld-linux";

typedef struct {
  void* beg; // [low, high)
  void* end;
} addr_pair_t;

#define tablecilk_sz 5
addr_pair_t tablecilk[tablecilk_sz];

#define tableother_sz 200
addr_pair_t tableother[tableother_sz];

//*************************** Forward Declarations **************************

static void
init_lcursor(lush_cursor_t* cursor);

static int 
determine_code_ranges();

static bool
is_libcilk(void* addr);

static bool
is_cilkprogram(void* addr);


// **************************************************************************
// Initialization/Finalization
// **************************************************************************

extern int
LUSHI_init(int argc, char** argv,
	   lush_agentid_t          aid,
	   LUSHCB_malloc_fn_t      malloc_fn,
	   LUSHCB_free_fn_t        free_fn,
	   LUSHCB_step_fn_t        step_fn,
	   LUSHCB_get_loadmap_fn_t loadmap_fn)
{
  MY_lush_aid = aid;

  CB_malloc      = malloc_fn;
  CB_free        = free_fn;
  CB_step        = step_fn;
  CB_get_loadmap = loadmap_fn;

  determine_code_ranges();
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
  // STUB
  return "";
}


// **************************************************************************
// Maintaining Responsibility for Code/Frame-space
// **************************************************************************

extern int 
LUSHI_reg_dlopen()
{
  determine_code_ranges();
  return 0;
}


extern bool 
LUSHI_ismycode(void* addr)
{
  return (is_libcilk(addr) || is_cilkprogram(addr));
}


int 
determine_code_ranges()
{
  LUSHCB_epoch_t* epoch;
  CB_get_loadmap(&epoch);

  // Initialize
  for (int i = 0; i < tablecilk_sz; ++i) {
    tablecilk[i].beg = NULL;
    tablecilk[i].end = NULL;
  }
  
  for (int i = 0; i < tableother_sz; ++i) {
    tableother[i].beg = NULL;
    tableother[i].end = NULL;
  }

  // Fill interval table
  int i_cilk = 0;
  int i_other = 0;
  csprof_epoch_module_t* mod;
  for (mod = epoch->loaded_modules; mod != NULL; mod = mod->next) {

    if (strstr(mod->module_name, libcilk_str)) {
      tablecilk[i_cilk].beg = mod->mapaddr;
      tablecilk[i_cilk].end = mod->mapaddr + mod->size;
      i_cilk++;

      if ( !(i_cilk < tablecilk_sz) ) {
	fprintf(stderr, "FIXME: libcilk has too many address intervals\n");
	i_cilk = tablecilk_sz - 1;
      }
    }

    if (strstr(mod->module_name, lib_str)
	|| strstr(mod->module_name, ld_str)) {
      tableother[i_other].beg = mod->mapaddr;
      tableother[i_other].end = mod->mapaddr + mod->size;
      i_other++;

      if ( !(i_other < tableother_sz) ) {
	fprintf(stderr, "FIXME: too many load modules\n");
	i_other = tableother_sz - 1;
      }
    }
  }

  return 0;
}


bool
is_libcilk(void* addr)
{
  for (int i = 0; i < tablecilk_sz; ++i) {
    if (!tablecilk[i].beg) { break; }
    if (tablecilk[i].beg <= addr && addr < tablecilk[i].end) {
      return true;
    }
  }
  return false;
}


bool
is_cilkprogram(void* addr)
{
  for (int i = 0; i < tableother_sz; ++i) {
    if (!tableother[i].beg) { break; }
    if (tableother[i].beg <= addr && addr < tableother[i].end) {
      return false;
    }
  }
  return true;
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

  bool is_cilkrt = is_libcilk(csr->u.ref_ip);
  bool is_user   = is_cilkprogram(csr->u.ref_ip);

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
      csr_flg_set(csr, UNW_FLG_SEEN_USER);
    }
    else if (is_cilkrt) {
      cur_seg = (deq_diff <= 1) ? UNW_SEG_CILKSCHED : UNW_SEG_CILKRT;

      // FIXME: sometimes the above test is not correct... OVERRIDE
      if (cur_seg == UNW_SEG_CILKRT && csr_flg_is(csr, UNW_FLG_SEEN_USER)) {
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
      case UNW_TY_MASTER:
	lush_cursor_set_assoc(cursor, LUSH_ASSOC_1_to_0);
	break;
      case UNW_TY_WORKER0:
	if (csr->u.prev_seg == UNW_SEG_USER 
	    && !csr_flg_is(csr, UNW_FLG_HAVE_LCTXT)) {
	  lush_cursor_set_assoc(cursor, LUSH_ASSOC_1_to_1);
	  csr_flg_set(csr, UNW_FLG_HAVE_LCTXT);
    	}
	else if (csr_flg_is(csr, UNW_FLG_HAVE_LCTXT)) {
	  lush_cursor_set_assoc(cursor, LUSH_ASSOC_0_to_0);
	}
	else {
	  lush_cursor_set_assoc(cursor, LUSH_ASSOC_1_to_0);
	}
	break;
      case UNW_TY_WORKER1:
	if (csr->u.prev_seg == UNW_SEG_USER 
	    && !csr_flg_is(csr, UNW_FLG_HAVE_LCTXT)) {
	  lush_cursor_set_assoc(cursor, LUSH_ASSOC_1_to_M);
	  csr_flg_set(csr, UNW_FLG_HAVE_LCTXT);
	}
	else if (csr_flg_is(csr, UNW_FLG_HAVE_LCTXT)) {
	  //TRY: lush_cursor_set_assoc(cursor, LUSH_ASSOC_0_to_0);
	  lush_cursor_set_assoc(cursor, LUSH_ASSOC_1_to_0);
	}
	else {
	  lush_cursor_set_assoc(cursor, LUSH_ASSOC_1_to_0);
	}
	break;
      default: fprintf(stderr, "FIXME: default case (assert)\n");
    }
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
    if (csr->u.after_beg_lnote) {
      ty = LUSH_STEP_END_CHORD;
    }
    else {
      cilk_ip_init(lip, csr->u.ref_ip /*0*/);
      ty = LUSH_STEP_CONT;
      csr->u.after_beg_lnote = true;
    }
  }
  else if (as == LUSH_ASSOC_1_to_M) {
    // INVARIANT: csr->u.cilk_closure is non-NULL
    Closure* cl = csr->u.cilk_closure;
    if (csr->u.after_beg_lnote) {
      cl = csr->u.cilk_closure = cl->parent;
      if (!cl) {
	cilk_ip_init(lip, NULL /*0*/);
	ty = LUSH_STEP_END_CHORD;
      }
      else {
	cilk_ip_init(lip, CILKFRM_PROC(cl->frame) /*cl->status*/);
	ty = LUSH_STEP_CONT;
      }
    }
    else {
      cilk_ip_init(lip, CILKFRM_PROC(cl->frame) /*cl->status*/);
      ty = LUSH_STEP_CONT;
      csr->u.after_beg_lnote = true;
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
  // STUB
  return 0;
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
    
    csr->u.prev_seg = UNW_SEG_NULL;

    // unw_ty_t
    if (!ws) {
      csr->u.ty = UNW_TY_MASTER;
    }
    else if (ws->self == 0 && !CILK_Has_Thread0_Stolen) {
      csr->u.ty = UNW_TY_WORKER0;
    }
    else {
      csr->u.ty = UNW_TY_WORKER1;
    }

    csr->u.flg = UNW_FLG_NULL;

    csr->u.cilk_worker_state = ws;
    csr->u.cilk_closure = (ws) ? CILKWS_CL_DEQ_BOT(ws) : NULL;
  }
  else if (aid_prev != MY_lush_aid) {
    fprintf(stderr, "FIXME: HOW TO HANDLE MULTIPLE AGENTS?\n");
  }

  // -------------------------------------------------------
  // intra-bichord data
  // -------------------------------------------------------
  memset(lip, 0, sizeof(*lip));

  csr->u.ref_ip = (void*)lush_cursor_get_ip(cursor);
  csr->u.after_beg_lnote = false;
}


// **************************************************************************
// 
// **************************************************************************

extern int
LUSHI_lip_destroy(lush_lip_t* lip)
{
  // STUB
  return 0;
}


extern int 
LUSHI_lip_eq(lush_lip_t* lip)
{
  // STUB
  return 0;
}


extern int
LUSHI_lip_read()
{
  // STUB
  return 0;
}


extern int
LUSHI_lip_write()
{
  // STUB
  return 0;
}


// **************************************************************************
// Concurrency
// **************************************************************************

extern int
LUSHI_has_concurrency()
{
  // STUB
  return 0;
}

extern int 
LUSHI_get_concurrency()
{
  // STUB
  return 0;
}

// **************************************************************************
