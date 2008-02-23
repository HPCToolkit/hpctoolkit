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
    void* ref_ip; // reference physical ip
    bool seen_cilkprog;
    bool is_beg_lnote;
  } u;
};


//*************************** Forward Declarations **************************

// FIXME: hackish implementation
static const char* libcilk_str = "libcilk";
static const char* lib_str = "lib";
static const char* ld_str = "ld-linux";

typedef struct {
  void* beg; // [low, high)
  void* end;
} addr_pair_t;

addr_pair_t tablecilk;

#define tableother_sz 20
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

// FIXME: should go away when unw_step is fixed
extern void *monitor_unwind_fence1,*monitor_unwind_fence2;
extern void *monitor_unwind_thread_fence1,*monitor_unwind_thread_fence2;

int HACK_is_fence(void *iip)
{
  void **ip = (void **) iip;

  return ((ip >= &monitor_unwind_fence1) && (ip <= &monitor_unwind_fence2)) ||
    ((ip >= &monitor_unwind_thread_fence1) && (ip <= &monitor_unwind_thread_fence2));
}

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
  int i;
  LUSHCB_epoch_t* epoch;
  CB_get_loadmap(&epoch);

  if (epoch->num_modules > 20) {
    fprintf(stderr, "FIXME: too many load modules");
  }

  // Initialize
  tablecilk.beg = NULL;
  tablecilk.end = NULL;
  
  for (i = 0; i < tableother_sz; ++i) {
    tableother[i].beg = NULL;
    tableother[i].end = NULL;
  }

  // Fill interval table
  i = 0;
  csprof_epoch_module_t* mod;
  for (mod = epoch->loaded_modules; mod != NULL; mod = mod->next) {

    if (strstr(mod->module_name, libcilk_str)) {
      if (tablecilk.beg != NULL) {
	fprintf(stderr, "FIXME: assuming only one address interval");
      }
      tablecilk.beg = mod->mapaddr;
      tablecilk.end = mod->mapaddr + mod->size;
    }

    if (strstr(mod->module_name, lib_str)
	|| strstr(mod->module_name, ld_str)) {
      tableother[i].beg = mod->mapaddr;
      tableother[i].end = mod->mapaddr + mod->size;
      i++;
    }
  }

  return 0;
}


bool
is_libcilk(void* addr)
{
  return (tablecilk.beg <= addr && addr < tablecilk.end);
}


bool
is_cilkprogram(void* addr)
{
  int i;
  for (i = 0; i < tableother_sz; ++i) {
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
  init_lcursor(cursor);
  cilk_cursor_t* csr = (cilk_cursor_t*)lush_cursor_get_lcursor(cursor);

  csr->u.ref_ip = (void*)lush_cursor_get_ip(cursor);
  bool seen_cilkprog = csr->u.seen_cilkprog;

  bool is_cilkrt   = is_libcilk(csr->u.ref_ip);
  bool is_cilkprog = is_cilkprogram(csr->u.ref_ip);

  bool is_TOS; // top of stack
  if (lush_cursor_is_flag(cursor, LUSH_CURSOR_FLAGS_BEG_PPROJ)) {
    is_TOS = true;
  }

  // FIXME: consider effects of multiple agents
  //LUSH_AGENTID_XXX_t last_aid = lush_cursor_get_aid(cursor); 

  // Given p-note derive l-note:
  //   1. is_cilkrt & is_TOS  => Cilk-scheduling or Cilk-overhead
  //   2. is_cilkrt & !is_TOS & seen_cilkprog  => Cilk-sched + logical stack
  //   3. is_cilkrt & !is_TOS & !seen_cilkprog => {result (1)}
  //   4. is_cilkprog => Cilk + logical Cilk
  if (is_cilkrt) {
    if (is_TOS) {
      // case (1)
      lush_cursor_set_assoc(cursor, LUSH_ASSOC_1_to_0);
    }
    else {
      if (seen_cilkprog) {
	// case (2)
	// FIXME: lush_cursor_set_assoc(cursor, LUSH_ASSOC_1_to_M)
	lush_cursor_set_assoc(cursor, LUSH_ASSOC_1_to_0);
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
  // NOTE: Since all associations are 1 <-> x, it is always valid to step.

  lush_step_t ty = LUSH_STEP_NULL;
  
  { // FIXME: temporary
    void* ip = (void*)lush_cursor_get_ip(cursor);
    if (HACK_is_fence(ip)) {
      return LUSH_STEP_END_PROJ;
    }
  }

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
  
  if (as == LUSH_ASSOC_1_to_0) {
    ty = LUSH_STEP_END_CHORD;
  }
  else if (as == LUSH_ASSOC_1_to_1) {
    if (csr->u.is_beg_lnote) {
      ty = LUSH_STEP_END_CHORD;
      csr->u.is_beg_lnote = false;
    }
    else {
      lip->ip = csr->u.ref_ip;
      ty = LUSH_STEP_CONT;
      csr->u.is_beg_lnote = true;
    }
  }
  else if (LUSH_ASSOC_1_to_M) {
    if (csr->u.is_beg_lnote) {
      // FIXME: advance lip;
      ty = (lip->ip == NULL) ? LUSH_STEP_END_CHORD : LUSH_STEP_CONT;
      csr->u.is_beg_lnote = false;
    }
    else {
      lip->ip = csr->u.ref_ip;
      ty = LUSH_STEP_CONT;
      csr->u.is_beg_lnote = true;
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
  lush_lcursor_t* csr = lush_cursor_get_lcursor(cursor);
  memset(csr, 0, sizeof(*csr));

  lush_lip_t* lip = lush_cursor_get_lip(cursor);
  memset(lip, 0, sizeof(*lip));
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
