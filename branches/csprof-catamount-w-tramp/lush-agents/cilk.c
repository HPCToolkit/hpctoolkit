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

#define LUSH_CB_DECL(FN) \
  FN ## _fn_t FN

LUSH_CB_DECL(LUSHCB_malloc);
LUSH_CB_DECL(LUSHCB_free);
LUSH_CB_DECL(LUSHCB_step);
LUSH_CB_DECL(LUSHCB_get_loadmap);

#undef LUSH_CB_DECL(FN)

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

export int
LUSHI_init(int argc, char** argv,
	   LUSHCB_malloc_fn_t      malloc_fn,
	   LUSHCB_free_fn_t        free_fn,
	   LUSHCB_step_fn_t        step_fn,
	   LUSHCB_get_loadmap_fn_t loadmap_fn)
{
  LUSHCB_malloc      = malloc_fn;
  LUSHCB_free        = free_fn;
  LUSHCB_step        = step_fn;
  LUSHCB_get_loadmap = loadmap_fn;
  
  determine_code_ranges();
  return 0;
}


export int 
LUSHI_fini()
{
  return 0;
}


export char* 
LUSHI_strerror(int code)
{
  // STUB
  return "";
}


// **************************************************************************
// Maintaining Responsibility for Code/Frame-space
// **************************************************************************

export int 
LUSHI_reg_dlopen()
{
  determine_code_ranges();
  return 0;
}


export bool 
LUSHI_ismycode(void* addr)
{
  return (libcilk_low_addr <= addr && addr < libcilk_high_addr);
}


int 
determine_code_ranges()
{
  LUSHCB_epoch_t* epoch;
  LUSHCB_get_loadmap(&epoch);
 
  libcilk_low_addr  = NULL;
  libcilk_high_addr = NULL;

  for (csprof_epoch_module_t* mod = epoch->loaded_modules; 
       mod != NULL; mod = mod->next) {
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
export lush_step_t
LUSHI_peek_bichord(lush_cursor_t& cursor)
{
  
}


// Given a lush_cursor with a valid bichord, determine the next pnote
// (or lnote)
export lush_step_t
LUSHI_step_pnote(lush_cursor_t* cursor)
{
  
}


export lush_step_t
LUSHI_step_lnote(lush_cursor_t* cursor)
{
  
}


export int 
LUSHI_set_active_frame_marker(ctxt, cb)
{
  // STUB
  return 0;
}


// --------------------------------------------------------------------------

export int
LUSHI_lip_destroy(lush_lip_t* lip)
{
  // STUB
  return 0;
}


export int 
LUSHI_lip_eq(lush_lip_t* lip)
{
  // STUB
  return 0;
}


export int
LUSHI_lip_read()
{
  // STUB
  return 0;
}


export int
LUSHI_lip_write()
{
  // STUB
  return 0;
}


// **************************************************************************
// Concurrency
// **************************************************************************

export int
LUSHI_has_concurrency()
{
  // STUB
  return 0;
}

export int 
LUSHI_get_concurrency()
{
  // STUB
  return 0;
}

// **************************************************************************
