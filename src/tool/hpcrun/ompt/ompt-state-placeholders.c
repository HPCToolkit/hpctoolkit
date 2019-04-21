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
// Copyright ((c)) 2002-2014, Rice University
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
// system includes 
//***************************************************************************

#include <assert.h>



//***************************************************************************
// local includes 
//***************************************************************************

#include <lib/prof-lean/placeholders.h>

#include <hpcrun/fnbounds/fnbounds_interface.h>
#include <hpcrun/safe-sampling.h>
#include <hpcrun/hpcrun-initializers.h>

#include "ompt-state-placeholders.h"



//***************************************************************************
// global variables 
//***************************************************************************

ompt_placeholders_t ompt_placeholders;



//***************************************************************************
// local variables 
//***************************************************************************

static closure_t ompt_placeholder_init_closure;



//***************************************************************************
// private operations
//***************************************************************************

//----------------------------------------------------------------------------
// placeholder functions for blame shift reporting
//----------------------------------------------------------------------------

void 
ompt_idle_state
(
  void
)
{
  // this function is a placeholder that represents the calling context of
  // idle OpenMP worker threads. It is not meant to be invoked.
}


void 
ompt_overhead_state
(
  void
)
{
  // this function is a placeholder that represents the calling context of
  // threads working in the OpenMP runtime.  It is not meant to be invoked.
}


void 
ompt_barrier_wait_state
(
  void
)
{
  // this function is a placeholder that represents the calling context of
  // threads waiting for a barrier in the OpenMP runtime. It is not meant 
  // to be invoked.
}


void 
ompt_task_wait_state
(
  void
)
{
  // this function is a placeholder that represents the calling context of
  // threads waiting for a task in the OpenMP runtime. It is not meant 
  // to be invoked.
}


void 
ompt_mutex_wait_state
(
  void
)
{
  // this function is a placeholder that represents the calling context of
  // threads waiting for a mutex in the OpenMP runtime. It is not meant 
  // to be invoked.
}


void 
ompt_region_unresolved
(
  void
)
{
  // this function is a placeholder that represents that the calling context 
  // of an OpenMP region is unresolved. This function is not meant to be
  // invoked
}


//***************************************************************************
// private operations
//***************************************************************************

static load_module_t *
pc_to_lm
(
  void *pc
)
{
  void *func_start_pc, *func_end_pc;
  load_module_t *lm = NULL;
  fnbounds_enclosing_addr(pc, &func_start_pc, &func_end_pc, &lm);
  return lm;
}


static void 
init_placeholder
(
  ompt_placeholder_t *p, 
  void *pc
)
{
  // protect against receiving a sample here. if we do, we may get 
  // deadlock trying to acquire a lock associated with 
  // fnbounds_enclosing_addr
  hpcrun_safe_enter();
  {
    void *cpc = canonicalize_placeholder(pc);
    p->pc = cpc;
    p->pc_norm = hpcrun_normalize_ip(cpc, pc_to_lm(cpc));
  }
  hpcrun_safe_exit();
}


//***************************************************************************
// interface operations
//***************************************************************************


#define OMPT_PLACEHOLDER_MACRO(f)               \
  {                                             \
    void * fn = (void *) ompt_fn_lookup(#f);    \
    if (!fn) fn = (void *) f ## _state;         \
    init_placeholder(&ompt_placeholders.f, fn); \
  }

void
ompt_init_placeholders_internal
(
  void *arg
)
{
  ompt_function_lookup_t ompt_fn_lookup = (ompt_function_lookup_t) arg;

  FOREACH_OMPT_PLACEHOLDER_FN(OMPT_PLACEHOLDER_MACRO)

  // placeholder for unresolved regions 
  //
  // this placeholder is used when the context for a parallel region remains 
  // unresolved at the end of a program execution. this can happen if a 
  // program execution is interrupted before the region is resolved. 
  init_placeholder(&ompt_placeholders.region_unresolved, ompt_region_unresolved);
}


// placeholders can only be initialized after fnbounds module is initialized.
// for this reason, defer placeholder initialization until the end of 
// hpcrun initialization.
void
ompt_init_placeholders
(
  ompt_function_lookup_t ompt_fn_lookup
)
{
  // initialize closure for initializer
  ompt_placeholder_init_closure.fn = ompt_init_placeholders_internal; 
  ompt_placeholder_init_closure.arg = ompt_fn_lookup;

  // register closure
  hpcrun_initializers_defer(&ompt_placeholder_init_closure);
}
