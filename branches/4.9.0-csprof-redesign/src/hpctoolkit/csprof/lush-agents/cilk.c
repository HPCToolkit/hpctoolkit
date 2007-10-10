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

//*************************** User Include Files ****************************

#include <lush/lushi.h>
#include <lush/lushi-cb.h>

//*************************** Forward Declarations **************************

#define LUSHCB_DECL(FN) \
  FN ## _fn_t  my ## FN

LUSHCB_DECL(LUSHCB_malloc);
LUSHCB_DECL(LUSHCB_free);
LUSHCB_DECL(LUSHCB_step);
LUSHCB_DECL(LUSHCB_get_loadmap);

#undef LUSHCB_DECL

//*************************** Forward Declarations **************************

static const char* libcilk = "libcilk";

// The libcilk address range [low, high)
static void* libcilk_low_addr;
static void* libcilk_high_addr;

static int 
determine_code_ranges();

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
  myLUSHCB_malloc      = malloc_fn;
  myLUSHCB_free        = free_fn;
  myLUSHCB_step        = step_fn;
  myLUSHCB_get_loadmap = loadmap_fn;
  
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
  return (libcilk_low_addr <= addr && addr < libcilk_high_addr);
}


int 
determine_code_ranges()
{
  LUSHCB_epoch_t* epoch;
  myLUSHCB_get_loadmap(&epoch);
 
  libcilk_low_addr  = NULL;
  libcilk_high_addr = NULL;

  csprof_epoch_module_t* mod;
  for (mod = epoch->loaded_modules; mod != NULL; mod = mod->next) {
    char* s = strstr(mod->module_name, libcilk);
    if (s) {
      if (libcilk_low_addr != NULL) {
	// FIXME: we currently assume only one range of addresses
	fprintf(stderr, "Internal Error: determine_code_ranges");
      }
      libcilk_low_addr  = mod->mapaddr;
      libcilk_high_addr = mod->mapaddr + mod->size;
    }
  }
  return 0;
}


// **************************************************************************
// Logical Unwinding
// **************************************************************************

// Given a lush_cursor with a valid pchord, compute bichord and
// lchord meta-information
extern lush_step_t
LUSHI_peek_bichord(lush_cursor_t* cursor)
{
  // FIXME
  return LUSH_STEP_ERROR;
}


// Given a lush_cursor with a valid bichord, determine the next pnote
// (or lnote)
extern lush_step_t
LUSHI_step_pnote(lush_cursor_t* cursor)
{
  // FIXME
  return LUSH_STEP_ERROR;
}


extern lush_step_t
LUSHI_step_lnote(lush_cursor_t* cursor)
{
  // FIXME
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
