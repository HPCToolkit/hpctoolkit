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

#include <include/gcc-attr.h>

#include "lush-pthread.h"

#include "pmsg.h"


//*************************** Forward Declarations **************************

// FIXME: used by GCC_ATTR_VAR_CACHE_ALIGN; move elsewhere
#define HOST_CACHE_LINE_SZ 64 /*L1*/

// NOTE: For a portable alternative, union each cache-aligned variable
// to with an char array of the appropriate size.

typedef struct {
   
  long ps_num_procs        GCC_ATTR_VAR_CACHE_ALIGN;
  long ps_num_threads;

  long ps_num_working      GCC_ATTR_VAR_CACHE_ALIGN;

  long ps_num_working_lock GCC_ATTR_VAR_CACHE_ALIGN;
  
  long ps_num_idle_cond    GCC_ATTR_VAR_CACHE_ALIGN;

#if (LUSH_PTHR_FN_TY == 3)
  BalancedTree_t syncObjToData; // synch-obj -> data
  long curSpinLockIdx;
  SpinLockData_t spinLockData[lushPthr_spinLockDataSZ];
#endif
  
} lushPthr_globals_t;


lushPthr_globals_t globals = {
  .ps_num_procs = 0,
  .ps_num_threads = 0,
  .ps_num_working = 0,
  .ps_num_working_lock = 0,
  .ps_num_idle_cond = 0
  // syncObjToData, curSpinLockIdx, spinLockData
};


// **************************************************************************
// 
// **************************************************************************

void 
lushPthr_processInit()
{
  // WARNING: At the moment, this routine is called *after*
  // lushPthr_init(), at least for the first thread.

  globals.ps_num_procs   = sysconf(_SC_NPROCESSORS_ONLN);
  globals.ps_num_threads = 0;
  
  globals.ps_num_working      = 0;
  globals.ps_num_working_lock = 0;

  globals.ps_num_idle_cond    = 0;

#if (LUSH_PTHR_FN_TY == 3)
  BalancedTree_init(&globals.syncObjToData);
  globals.curSpinLockIdx = 0;
#endif
}


// **************************************************************************

void 
lushPthr_init(lushPthr_t* x)
{
  x->is_working = false;
  x->num_locks  = 0;
  x->cond_lock  = 0;
  
  x->doIdlenessCnt = 0;
  x->begIdleness = 0;
  x->idleness    = 0;
  
  x->ps_num_procs   = &globals.ps_num_procs;
  x->ps_num_threads = &globals.ps_num_threads;

  x->ps_num_working      = &globals.ps_num_working;
  x->ps_num_working_lock = &globals.ps_num_working_lock;
  
  x->ps_num_idle_cond = &globals.ps_num_idle_cond;

#if (LUSH_PTHR_FN_TY == 3)
  x->syncObjToData  = &globals.syncObjToData;
  x->curSyncObjData = NULL;

  x->curSpinLockIdx = &globals.curSpinLockIdx;
  x->spinLockData = globals.spinLockData;
#endif
}


void 
lushPthr_dump(lushPthr_t* x, const char* nm)
{
  EMSG("lushPthr/%s:\t is_wrking %d, num_lck %d, cnd_lck %d | "
       "# wrking %ld, wrking_lck %ld, idle_cnd %ld | "
       "# procs %ld, threads %d",
       nm, x->is_working, x->num_locks, x->cond_lock,
       *x->ps_num_working, *x->ps_num_working_lock, *x->ps_num_idle_cond,
       *x->ps_num_procs, *x->ps_num_threads);
}


//***************************************************************************
