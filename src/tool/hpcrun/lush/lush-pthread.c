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
//   $HeadURL$
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

#include <messages/messages.h>


//*************************** Forward Declarations **************************

#if (LUSH_PTHR_FN_TY == 3)
#  define lushPthr_memSizeElem (5 * 1024 * 1024)
#else
#  define lushPthr_memSizeElem (0)
#endif

// FIXME: tallent: hardcoded size for now
lushPtr_SyncObjData_t lushPthr_mem[lushPthr_memSizeElem] GCC_ATTR_VAR_CACHE_ALIGN;

#define lushPthr_memSize    (sizeof(lushPthr_mem))
#define lushPthr_memSizeMax (0x3fffffff) /* lower 30 bits */

void* lushPthr_mem_beg;
void* lushPthr_mem_end;

void* lushPthr_mem_ptr;


#if (LUSH_DBG_STATS)
long DBG_numLockAcq        = 0; // total lock acquires

long DBG_numLockAlloc      = 0; // total locks allocated

long DBG_maxLockAllocCur    = 0; // max locks allocated simultaneously
long DBG_numLockFreelistCur = 0; // number of (spin) locks cur. on freelists
#endif


//*************************** Forward Declarations **************************

// NOTE: For a portable alternative, union each cache-aligned variable
// to with an char array of the appropriate size.

typedef struct {
   
  long ps_num_procs        GCC_ATTR_VAR_CACHE_ALIGN;
  long ps_num_threads;

  long ps_num_working      GCC_ATTR_VAR_CACHE_ALIGN;

  long ps_num_working_lock GCC_ATTR_VAR_CACHE_ALIGN;
  
  long ps_num_idle_cond    GCC_ATTR_VAR_CACHE_ALIGN;

  // LUSH_PTHR_FN_TY == 3
  BalancedTree_t ps_syncObjToData; // synch-obj -> data
  
} lushPthr_globals_t;


lushPthr_globals_t globals = {
  .ps_num_procs = 0,
  .ps_num_threads = 0,
  .ps_num_working = 0,
  .ps_num_working_lock = 0,
  .ps_num_idle_cond = 0
  // ps_syncObjToData
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

  // LUSH_PTHR_FN_TY == 3
  BalancedTree_init(&globals.ps_syncObjToData, hpcrun_malloc, 
		    sizeof(lushPtr_SyncObjData_t));

  lushPthr_mem_beg = (void*)lushPthr_mem;
  lushPthr_mem_end = (void*)lushPthr_mem + lushPthr_memSize;

  // align with next cache line
  lushPthr_mem_ptr = (void*)( (uintptr_t)(lushPthr_mem_beg 
					  + lushPthr_maxValueOfLock
					  + (HOST_CACHE_LINE_SZ - 1))
			      & (uintptr_t)~(HOST_CACHE_LINE_SZ - 1) );
  
#if (LUSH_PTHR_FN_TY == 3)
  // sanity check
  if ( !(sizeof(pthread_spinlock_t) == 4) ) {
    hpcrun_abort("LUSH Pthreads found unexpected pthread_spinlock_t type!");
  }
  if ( !(lushPthr_memSize < lushPthr_memSizeMax) ) {
    hpcrun_abort("LUSH Pthreads found bad mem size!");
  }
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

  // ------------------------------------------------------------
  // LUSH_PTHR_FN_TY == 3
  // ------------------------------------------------------------
  x->ps_syncObjToData = &globals.ps_syncObjToData;
  BalancedTree_init(&x->syncObjToData, hpcrun_malloc, 0/*nodeDataSz*/);

  x->syncObjData = NULL;
  QueuingRWLockLcl_init(&x->locklcl);

  x->cache_syncObj = NULL;
  x->cache_syncObjData = NULL;

  x->freelstHead = NULL;
  x->freelstTail = NULL;
}


void 
lushPthr_dump(lushPthr_t* x, const char* nm, void* lock)
{
#if (LUSH_PTHR_FN_TY == 3) 
  int lckval = (lock) ? *((int*)lock) : 0;
  int lushval = 0;
  if (lushPthr_isSyncDataPointer(lckval)) {
    lushPtr_SyncObjData_t* data = lushPthr_getSyncDataPointer(lckval);
    lushval = data->lock.spin;
  }
  EMSG("lushPthr/%s:\t lck: %p->%#x->%d", nm, lock, lckval, lushval);
#else
  EMSG("lushPthr/%s:\t is_wrking %d, num_lck %d, cnd_lck %d | "
       "# wrking %ld, wrking_lck %ld, idle_cnd %ld | "
       "# procs %ld, threads %d",
       nm, x->is_working, x->num_locks, x->cond_lock,
       *x->ps_num_working, *x->ps_num_working_lock, *x->ps_num_idle_cond,
       *x->ps_num_procs, *x->ps_num_threads);
#endif
}



//***************************************************************************
