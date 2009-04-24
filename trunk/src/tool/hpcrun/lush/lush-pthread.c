// -*-Mode: C++;-*- // technically C99
// $Id$

//***************************************************************************
//
// File: 
//   $Source$
//
// Purpose:
//   LUSH: Logical Unwind Support for HPCToolkit
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
// Author:
//   Nathan Tallent, Rice University.
//
//***************************************************************************

//************************* System Include Files ****************************

#include <stdlib.h>
#include <stdbool.h>

#include <unistd.h> // sysconf


//*************************** User Include Files ****************************

#include "lush-pthread.h"


//*************************** Forward Declarations **************************

long lush_pthr_ps_num_procs   = 0;
long lush_pthr_ps_num_threads = 0;

long lush_pthr_ps_num_working = 0;
long lush_pthr_ps_num_working_lock = 0;

long lush_pthr_ps_num_idle_cond = 0;


// **************************************************************************
// 
// **************************************************************************

void 
lush_pthreads__init()
{
  lush_pthr_ps_num_procs   = sysconf(_SC_NPROCESSORS_ONLN);
  lush_pthr_ps_num_threads = 0;
  
  lush_pthr_ps_num_working      = 0;
  lush_pthr_ps_num_working_lock = 0;

  lush_pthr_ps_num_idle_cond    = 0;
}


// **************************************************************************

void 
lush_pthr__init(lush_pthr_t* x)
{
  x->is_working = false;
  x->num_locks  = 0;
  x->cond_lock  = 0;
  
  x->ps_num_procs   = &lush_pthr_ps_num_procs;
  x->ps_num_threads = &lush_pthr_ps_num_threads;

  x->ps_num_working      = &lush_pthr_ps_num_working;
  x->ps_num_working_lock = &lush_pthr_ps_num_working_lock;
  
  x->ps_num_idle_cond = &lush_pthr_ps_num_idle_cond;
}


//***************************************************************************
