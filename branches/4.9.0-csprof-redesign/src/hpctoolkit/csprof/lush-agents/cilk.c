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

//*************************** Forward Declarations **************************

#define LUSHCB_DECL(FN) \
 LUSH ## FN ## _fn_t  FN

LUSHCB_DECL(CB_malloc);
LUSHCB_DECL(CB_free);
LUSHCB_DECL(CB_step);
LUSHCB_DECL(CB_get_loadmap);
// lush_cursor stuff

#undef LUSHCB_DECL

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


// **************************************************************************
// Initialization/Finalization
// **************************************************************************

extern int
LUSHI_init(int argc, char** argv,
	   LUSHCB_malloc_fn_t      malloc_fn,
	   LUSHCB_free_fn_t        free_fn,
	   LUSHCB_step_fn_t        step_fn,
	   LUSHCB_get_loadmap_fn_t loadmap_fn)
{
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
LUSHI_peek_bichord(lush_cursor_t* cursor)
{
#if 0
  // INVARIANTS for the libcilk DSO simplified model:

  // FIXME: for now we will assume there is always a 1-to-1 or 1-to-0 associativity.


  // 1. LUSHI_ismycode(ip) holds, where ip is the physical ip of pchord.
  //
  // 2. libcilk implies the pnote is one of
  //    - Cilk runtime [only n top frames] = no lnote => 1-to-0
  //    - Cilk scheduler                   = no lnote => 1-to-0
  //
  //    !libother implies the pnote is in the app (given basic assumptions)
  
  // 3. bottom of the stack is a closure from which we can find stack
  //    while walking stack, the first non-code function or
  //    ...

  void* ip = (void*)lush_cursor_get_ip(cursor);
  bool is_cilkrt   = is_libcilk(ip);
  bool is_cilkprog = !is_cilkrt; // cf. LUSHI_ismycode

  bool is_TOS; // top of stack
  if (lush_cursor_is_flag(cursor, LUSH_CURSOR_FLAGS_INIT)) {
    is_TOS = true;
  }

  
  if (is_cilkrt) {
    if (is_TOS) {
      // could be either scheduling or runtime. 
      // in either case, we don't need a logical...
      // is_TOS  => ***(rr-help or scheduling)***
    }
    else {
      if (have-seen-cilk-prog) {
	we are in scheduling routines --> look for anscestors
      }
      else {
	we are in scheduling or rt routines --> no need to look for ancestors
      }
    // !is_TOS && have-seen-cilk-prog  => scheduling
    }
  }
  // is_cilkrt && is_TOS && have note seen ==> 
  // is_cilkrt && !is_TOS ==>
    
  // we know the top of the stack has stuff

  // we need to identify the cilk fast and slow procedures too

      - a Cilk runtime routine [can detect]
      - a Cilk fast routine
      - The Cilk scheduler code / loop

  // have to figure out the assoc at this point...
  


  LUSHI_ismycode(ip);

  // if within cilk AND RT support -> 

  // pnote and lnote may need to consult this...

  // FIXME
#endif
  return LUSH_STEP_ERROR;
}


extern lush_step_t
LUSHI_step_pnote(lush_cursor_t* cursor)
{
  // FIXME: this becomes a force-step

  lush_step_t ty = LUSH_ASSOC_NULL;

  lush_assoc_t assoc = lush_cursor_get_assoc(cursor);

  if (assoc == LUSH_ASSOC_0_to_1_n) {
    ty = LUSH_STEP_DONE;
    return ty;
  }

  int t = CB_step(lush_cursor_get_pcursor(cursor));
  if (t > 0) {
    // LUSH_STEP_CONT
    ty = LUSH_STEP_DONE; // FIXME: pchord always contains on pnote
  }
  else if (t == 0) {
    ty = LUSH_STEP_DONE;
  }
  else if (t < 0) {
    ty = LUSH_STEP_ERROR;
  }
  
  return ty;
}


extern lush_step_t
LUSHI_step_lnote(lush_cursor_t* cursor)
{
  // FIXME: must account for *lchord* and associativity
  return LUSH_STEP_ERROR;
}


extern int 
LUSHI_set_active_frame_marker(ctxt, cb)
{
  // STUB
  return 0;
}


// --------------------------------------------------------------------------

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
