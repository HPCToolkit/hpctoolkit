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
//    $HeadURL$
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

#include <string.h>

#include <assert.h>

//*************************** User Include Files ****************************

#include "agent-tbb.h"

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
  // NOTE: Currently, this does not prevent our LUSHI_do_backtrace
  // from being called, but it may not be quite right in the context
  // of multiple agents.
  return false; // force LUSH to use the identity logical unwind
}


// **************************************************************************
// 
// **************************************************************************

extern lush_step_t
LUSHI_step_bichord(lush_cursor_t* cursor)
{
  assert(0 && "LUSHI_step_bichord: should never be called");
  return LUSH_STEP_ERROR;
}


extern lush_step_t
LUSHI_step_pnote(lush_cursor_t* cursor)
{
  assert(0 && "LUSHI_step_pnote: should never be called");
  return LUSH_STEP_ERROR;
}


extern lush_step_t
LUSHI_step_lnote(lush_cursor_t* cursor)
{
  assert(0 && "LUSHI_step_lnote: should never be called");
  return LUSH_STEP_ERROR;
}


extern int 
LUSHI_set_active_frame_marker(/*ctxt, cb*/)
{
  return 0; // STUB
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
  *doMetric = true;
  *doMetricIdleness = false;
  *incrMetric = incrMetricIn;
  *incrMetricIdleness = 0.0;

  return *doMetric;
}


// **************************************************************************
