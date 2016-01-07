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

#ifndef lush_pthreads_i
#define lush_pthreads_i

//************************* System Include Files ****************************

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

//*************************** User Include Files ****************************

#include <lib/prof-lean/atomic.h>
#include <lib/prof-lean/BalancedTree.h>
#include <lib/prof-lean/QueuingRWLock.h>

//*************************** Forward Declarations **************************

//***************************************************************************

#if defined(__cplusplus)
extern "C" {
#endif

//***************************************************************************

#define LUSH_DBG_STATS 0

// FIXME: used by GCC_ATTR_VAR_CACHE_ALIGN; move elsewhere
#define HOST_CACHE_LINE_SZ 64 /*L1*/

//***************************************************************************

#if (LUSH_DBG_STATS)
extern long DBG_numLockAcq;
extern long DBG_numLockAlloc;
extern long DBG_maxLockAllocCur;
extern long DBG_numLockFreelistCur;
#endif


typedef struct lushPtr_SyncObjData {

  union {
    pthread_spinlock_t spin; // usually an int
    //pthread_mutex_t  mutexlock
  } lock GCC_ATTR_VAR_CACHE_ALIGN;

  uint64_t idleness GCC_ATTR_VAR_CACHE_ALIGN;
  void*    cct_node;
  bool isBlockingWork;
  bool isLocked;

  struct lushPtr_SyncObjData* next;
  
} lushPtr_SyncObjData_t;


static inline void 
lushPtr_SyncObjData_init(lushPtr_SyncObjData_t* x)
{
  x->idleness = 0;
  x->cct_node = NULL;
  x->isBlockingWork = false;
  x->isLocked = false;

  x->next = NULL;
}


//***************************************************************************
// 
//***************************************************************************

typedef struct lushPthr {
  
  // -------------------------------------------------------
  // thread specific metrics
  // -------------------------------------------------------
  bool is_working; // thread is working (not blocked for any reason)
  int  num_locks;  // number of thread's locks (including a cond-var lock)
  int  cond_lock;  // 0 or the lock number on entry to cond-var critial region

  uint64_t doIdlenessCnt;
  uint64_t begIdleness; // begin idleness 'timestamp'
  uint64_t idleness;    // accumulated idleness

  // -------------------------------------------------------
  // process wide metrics
  // -------------------------------------------------------
  long* ps_num_procs;        // available processor cores
  long* ps_num_threads;

  long* ps_num_working;      // working (W_l) + (W_c) + (W_o)
  long* ps_num_working_lock; // working (W_l)
  //    ps_num_working_othr  // working (W_c) + (W_o)

  //    ps_num_idle_lock     // idleness (I_l)
  long* ps_num_idle_cond;    // idleness (I_cl) + (I_cv)

  // -------------------------------------------------------
  // LUSH_PTHR_FN_TY == 3
  // -------------------------------------------------------
  BalancedTree_t*        ps_syncObjToData; // synch-obj -> data
  BalancedTree_t         syncObjToData;    // synch-obj -> data

  lushPtr_SyncObjData_t* syncObjData;
  QueuingRWLockLcl_t     locklcl;

  void*                  cache_syncObj;
  lushPtr_SyncObjData_t* cache_syncObjData;

  lushPtr_SyncObjData_t* freelstHead;
  lushPtr_SyncObjData_t* freelstTail;
  
} lushPthr_t;
  
  
//***************************************************************************

extern void* lushPthr_mem_beg; // memory begin
extern void* lushPthr_mem_end; // memory end
extern void* lushPthr_mem_ptr; // current pointer


static inline void* 
lushPthr_malloc(size_t size) 
{
  void* memEnd = hpcrun_atomicAdd(&lushPthr_mem_ptr, size);
  if (memEnd < lushPthr_mem_end) {
    return (memEnd - size);
  }
  return NULL;
}
 

// **************************************************************************

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* lush_pthreads_i */
