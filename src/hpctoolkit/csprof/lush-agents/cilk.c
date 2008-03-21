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

//*************************** User Include Files ****************************

#include <lush/lushi.h>
#include <lush/lushi-cb.h>

#include <general.h> // FIXME: for MSG -- but should not include

//*************************** Forward Declarations **************************

#include <pthread.h>

#include <../cilk/cilk.h> /* Cilk */
// Cilk runtime
extern pthread_key_t CILK_WorkerState_key;

//*************************** Forward Declarations **************************

#define LUSHCB_DECL(FN) \
 LUSH ## FN ## _fn_t  FN

LUSHCB_DECL(CB_malloc);
LUSHCB_DECL(CB_free);
LUSHCB_DECL(CB_step);
LUSHCB_DECL(CB_get_loadmap);
// lush_cursor stuff

#undef LUSHCB_DECL

LUSH_AGENTID_XXX_t lush_aid;

//*************************** Forward Declarations **************************

typedef union cilk_ip cilk_ip_t;

union cilk_ip {
  // ------------------------------------------------------------
  // LUSH type
  // ------------------------------------------------------------
  lush_lip_t official_lip;
  
  // ------------------------------------------------------------
  // superimposed with:
  // ------------------------------------------------------------
  void* ip;
};


typedef union cilk_cursor cilk_cursor_t;

union cilk_cursor {
  // ------------------------------------------------------------
  // LUSH type
  // ------------------------------------------------------------
  lush_lcursor_t official_cursor;

  // ------------------------------------------------------------
  // superimposed with:
  // ------------------------------------------------------------
  struct {
    // ---------------------------------
    // inter-bichord data
    // ---------------------------------
    CilkWorkerState* cilk_worker_state;
    bool seen_cilkprog;

    // ---------------------------------
    // intra-bichord data
    // ---------------------------------
    void* ref_ip;          // reference physical ip
    bool after_beg_lnote;
    volatile CilkStackFrame** cilk_frame;

  } u;
};

#define CILK_WS_HEAD_PTR(/* CilkWorkerState* */ x) ((x)->cache.head)
#define CILK_WS_TAIL_PTR(/* CilkWorkerState* */ x) ((x)->cache.tail)

#define CILK_FRAME(/* CilkStackFrame** */ x)      (*(x))
#define CILK_FRAME_PROC(/* CilkStackFrame** */ x) ((**(x)).sig[0].inlet)

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

static int 
determine_code_ranges();

static bool
is_libcilk(void* addr);

static bool
is_cilkprogram(void* addr);

//*************************** Forward Declarations **************************

static void
init_lcursor(lush_cursor_t* cursor);

//*************************** Forward Declarations **************************


// **************************************************************************
// Initialization/Finalization
// **************************************************************************

extern int
LUSHI_init(int argc, char** argv,
	   LUSH_AGENTID_XXX_t aid,
	   LUSHCB_malloc_fn_t      malloc_fn,
	   LUSHCB_free_fn_t        free_fn,
	   LUSHCB_step_fn_t        step_fn,
	   LUSHCB_get_loadmap_fn_t loadmap_fn)
{
  lush_aid = aid;

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
  // Collect predicates
  // -------------------------------------------------------
  bool seen_cilkprog = csr->u.seen_cilkprog;

  bool is_cilkrt   = is_libcilk(csr->u.ref_ip);
  bool is_cilkprog = is_cilkprogram(csr->u.ref_ip);

  bool is_TOS; // top of stack
  if (lush_cursor_is_flag(cursor, LUSH_CURSOR_FLAGS_BEG_PPROJ)) {
    is_TOS = true;
  }

  // FIXME: consider effects of multiple agents
  //LUSH_AGENTID_XXX_t last_aid = lush_cursor_get_aid(cursor); 

  // -------------------------------------------------------
  // Given p-note derive l-note:
  //   1. is_cilkrt & is_TOS  => Cilk-scheduling or Cilk-overhead
  //   2. is_cilkrt & !is_TOS & seen_cilkprog  => Cilk-sched + logical stack
  //   3. is_cilkrt & !is_TOS & !seen_cilkprog => {result (1)}
  //   4. is_cilkprog => Cilk + logical Cilk
  // -------------------------------------------------------
  if (is_cilkrt) {
    if (is_TOS) {
      // case (1)
      lush_cursor_set_assoc(cursor, LUSH_ASSOC_1_to_0);
    }
    else {
      if (seen_cilkprog) {
	// case (2)
	lush_cursor_set_assoc(cursor, LUSH_ASSOC_1_to_M);
      }
      else {
	// case (3)
	lush_cursor_set_assoc(cursor, LUSH_ASSOC_1_to_0);
      }
    }
  }
  if (is_cilkprog) {
    // case (4)
    lush_cursor_set_assoc(cursor, LUSH_ASSOC_1_to_1);
    csr->u.seen_cilkprog = true;
  }

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
      lip->ip = csr->u.ref_ip;
      ty = LUSH_STEP_CONT;
      csr->u.after_beg_lnote = true;
    }
  }
  else if (LUSH_ASSOC_1_to_M) {
    if (csr->u.after_beg_lnote) {
      // INVARIANT csr->u.cilk_frame is non-NULL
      
      --(csr->u.cilk_frame); // Advance frame (moves toward the head)
      if (csr->u.cilk_frame < CILK_WS_HEAD_PTR(csr->u.cilk_worker_state)) {
	lip->ip = NULL;
	ty = LUSH_STEP_END_CHORD;
      }
      else {
	lip->ip = CILK_FRAME_PROC(csr->u.cilk_frame);
	ty = LUSH_STEP_CONT;
      }
    }
    else {
      lip->ip = CILK_FRAME_PROC(csr->u.cilk_frame);
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
  
  // -------------------------------------------------------
  // inter-bichord data
  // -------------------------------------------------------
  if (lush_cursor_is_flag(cursor, LUSH_CURSOR_FLAGS_BEG_PPROJ)) {
    csr->u.cilk_worker_state = 
      (CilkWorkerState*)pthread_getspecific(CILK_WorkerState_key);
    csr->u.seen_cilkprog = false;
  }

  // -------------------------------------------------------
  // intra-bichord data
  // -------------------------------------------------------
  memset(lip, 0, sizeof(*lip));

  csr->u.ref_ip = (void*)lush_cursor_get_ip(cursor);
  csr->u.after_beg_lnote = false;
  csr->u.cilk_frame = ((csr->u.cilk_worker_state) ? 
		       CILK_WS_TAIL_PTR(csr->u.cilk_worker_state) : NULL);
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
