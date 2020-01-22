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
// Copyright ((c)) 2002-2020, Rice University
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

#include "ompt-placeholders.h"



//***************************************************************************
// macros
//***************************************************************************


#define initialize_placeholder(f)               \
  {                                             \
    void * fn = (void *) f;			\
    init_placeholder(&ompt_placeholders.f, fn); \
  }

#define declare_placeholder_function(fn)	\
  void fn(void) { }



//***************************************************************************
// global variables 
//***************************************************************************

ompt_placeholders_t ompt_placeholders;



//***************************************************************************
// local variables 
//***************************************************************************

static closure_t ompt_placeholder_init_closure;



//***************************************************************************
// placeholder functions
//***************************************************************************

//----------------------------------------------------------------------------
// Definitions of placeholder functions used to represent an OpenMP
// state, operation, or parallel region. These placeholder functions
// are not meant to be invoked. Metrics associated with a placeholder
// are associated with the state, operation, or parallel region in a
// user's application. Metrics associated with a placeholder do not
// represent resource consumption by HPCToolkit.
//
// Placeholder functions are defined as global functions because we
// have seen optimizers remove names of static functions. We need
// placeholder names to be preserved so that we can rewrite them into
// strings that will be visible in hpcviewer and hpctraceviewer.
// ----------------------------------------------------------------------------

FOREACH_OMPT_PLACEHOLDER_FN(declare_placeholder_function)



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
  p->pc = canonicalize_placeholder(pc);
  p->pc_norm = hpcrun_normalize_ip(p->pc, pc_to_lm(p->pc));
}


static void
ompt_init_placeholders_internal
(
 void *arg
)
{
  // protect against receiving a sample here. if we do, we may get 
  // deadlock trying to acquire a lock associated with 
  // fnbounds_enclosing_addr
  hpcrun_safe_enter();

  FOREACH_OMPT_PLACEHOLDER_FN(initialize_placeholder)

  hpcrun_safe_exit();
}


//***************************************************************************
// interface operations
//***************************************************************************

// placeholders can only be initialized after fnbounds module is initialized.
// for this reason, defer placeholder initialization until the end of 
// hpcrun initialization.
void
ompt_init_placeholders
(
  void
)
{
  // initialize closure for initializer
  ompt_placeholder_init_closure.fn = ompt_init_placeholders_internal; 
  ompt_placeholder_init_closure.arg = 0;

  // register closure
  hpcrun_initializers_defer(&ompt_placeholder_init_closure);
}
