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

#ifndef lush_pthreads_h
#define lush_pthreads_h

//************************* System Include Files ****************************

#include <stdlib.h>
#include <stdbool.h>

#include <assert.h>

//*************************** User Include Files ****************************

#include <include/gcc-attr.h>
#include <include/min-max.h>

#include "lush-pthread.i"
#include "lush-backtrace.h" // for 'lush_agents'

#include <safe-sampling.h>
#include <sample_event.h>
#include <cct/cct.h>

#include <metrics.h>

#include <lib/prof-lean/atomic.h>
#include <lib/prof-lean/BalancedTree.h>

#include <lib/support-lean/timer.h>


//*************************** Forward Declarations **************************

#define LUSH_PTHR_FN_TY 1

#define LUSH_PTHR_DBG 0


//*************************** Forward Declarations **************************

#if defined(__cplusplus)
extern "C" {
#endif

//***************************************************************************
// 
//***************************************************************************

void 
lushPthr_processInit();

void
lushPthr_init(lushPthr_t* x);

void
lushPthr_dump(lushPthr_t* x, const char* nm, void* lock);


#define LUSH_PTHR_FN_REAL0(FN, TY) FN ## _ty ## TY
#define LUSH_PTHR_FN_REAL1(FN, TY) LUSH_PTHR_FN_REAL0(FN, TY)
#define LUSH_PTHR_FN(FN)           LUSH_PTHR_FN_REAL1(FN, LUSH_PTHR_FN_TY)

//***************************************************************************
// 1. Attribute a thread's idleness to victim (itself)
//***************************************************************************

#define LUSH_PTHR_SYNC_SMPL_PERIOD 33 /* if !0, sample synchronously */

static inline void
lushPthr_begSmplIdleness(lushPthr_t* x)
{
#if (LUSH_PTHR_SYNC_SMPL_PERIOD)
  x->doIdlenessCnt++;
  if (x->doIdlenessCnt == LUSH_PTHR_SYNC_SMPL_PERIOD) {
    uint64_t time = UINT64_MAX;
    time_getTimeReal(&time);
    x->begIdleness = time;
  }
#endif
}


static inline void
lushPthr_endSmplIdleness(lushPthr_t* x)
{
#if (LUSH_PTHR_SYNC_SMPL_PERIOD)
  if (x->doIdlenessCnt == LUSH_PTHR_SYNC_SMPL_PERIOD) {
    uint64_t time = 0;
    time_getTimeReal(&time);
    x->idleness += LUSH_PTHR_SYNC_SMPL_PERIOD * MAX(0, time - x->begIdleness);
    x->doIdlenessCnt = 0;
  }
#endif
}


static inline cct_node_t*
lushPthr_attribToCallPath(uint64_t idlenessIncr)
{
  sample_val_t smpl;

  hpcrun_safe_enter();

  ucontext_t context;
  getcontext(&context); // FIXME: check for errors
  smpl = hpcrun_sample_callpath(&context, lush_agents->metric_time,
				0/*metricIncr*/, 1/*skipInner*/, 1/*isSync*/);
  hpcrun_safe_exit();

  return smpl.sample_node;
}


//***************************************************************************

static inline void
lushPthr_thread_init_ty1(lushPthr_t* x)
{
  x->is_working = true;
}


static inline void
lushPthr_thread_fini_ty1(lushPthr_t* x)
{
  x->is_working = false;
}


static inline void
lushPthr_mutexLock_pre_ty1(lushPthr_t* restrict x, 
			   pthread_mutex_t* restrict lock)
{
  // N.B.: There is a small window where we could be sampled.  Rather
  // than setting an 'ignore-sample' flag, we currently depend upon
  // this happening infrequently.
  x->is_working = false;
  lushPthr_begSmplIdleness(x);
}


static inline void
lushPthr_mutexLock_post_ty1(lushPthr_t* restrict x, 
			    pthread_mutex_t* restrict lock)
{
  lushPthr_endSmplIdleness(x);
  x->is_working = true;
  if (x->idleness > 0) {
    lushPthr_attribToCallPath(x->idleness);
  }
}


static inline void
lushPthr_mutexTrylock_post_ty1(lushPthr_t* restrict x, 
			       pthread_mutex_t* restrict lock)
{
  x->is_working = true; // same
}


static inline void
lushPthr_mutexUnlock_post_ty1(lushPthr_t* restrict x, 
			      pthread_mutex_t* restrict lock)
{
  x->is_working = true; // same
}


static inline pthread_spinlock_t*
lushPthr_spinLock_pre_ty1(lushPthr_t* restrict x,
			  pthread_spinlock_t* restrict lock)
{
  x->is_working = false;
  return lock;
}


static inline void
lushPthr_spinLock_post_ty1(lushPthr_t* restrict x,
			   pthread_spinlock_t* restrict lock)
{
  x->is_working = true;
}


static inline pthread_spinlock_t*
lushPthr_spinTrylock_pre_ty1(lushPthr_t* restrict x, 
			     pthread_spinlock_t* restrict lock)
{
  return lock;
}


static inline void
lushPthr_spinTrylock_post_ty1(lushPthr_t* restrict x, 
			      pthread_spinlock_t* restrict lock)
{
  x->is_working = true; // same
}


static inline pthread_spinlock_t*
lushPthr_spinUnlock_pre_ty1(lushPthr_t* restrict x, 
			    pthread_spinlock_t* restrict lock)
{
  return lock;
}


static inline void
lushPthr_spinUnlock_post_ty1(lushPthr_t* restrict x, 
			     pthread_spinlock_t* restrict lock)
{
  x->is_working = true; // same
}


static inline pthread_spinlock_t*
lushPthr_spinDestroy_pre_ty1(lushPthr_t* restrict x, 
			     pthread_spinlock_t* restrict lock)
{
  return lock;
}


static inline void
lushPthr_spinDestroy_post_ty1(lushPthr_t* restrict x, 
			      pthread_spinlock_t* restrict lock)
{
}


static inline void
lushPthr_condwait_pre_ty1(lushPthr_t* x)
{
  x->is_working = false;
  lushPthr_begSmplIdleness(x);
}


static inline void
lushPthr_condwait_post_ty1(lushPthr_t* x)
{
  lushPthr_endSmplIdleness(x);
  x->is_working = true;
  if (x->idleness > 0) {
    lushPthr_attribToCallPath(x->idleness);
  }
}


//***************************************************************************
// 2. Attribute idleness to suspects (threads holding locks)
//***************************************************************************

static inline bool
lushPthr_isDirectlyInCond(lushPthr_t* x)
{
  return (x->cond_lock != 0 && x->cond_lock == x->num_locks);
}


static inline bool
lushPthr_isWorking_lock(lushPthr_t* x)
{
  return (x->is_working && x->num_locks > 0 && !lushPthr_isDirectlyInCond(x));
}


static inline bool
lushPthr_isWorking_cond(lushPthr_t* x)
{
  return (x->is_working && lushPthr_isDirectlyInCond(x));
}

//***************************************************************************

static inline void
lushPthr_thread_init_ty2(lushPthr_t* x)
{
  x->is_working = true;
  x->num_locks  = 0;
  x->cond_lock  = 0;

  hpcrun_atomicIncr(x->ps_num_threads);

  hpcrun_atomicIncr(x->ps_num_working);
  // x->ps_num_working_lock: same

  // x->ps_num_idle_cond: same
}


static inline void
lushPthr_thread_fini_ty2(lushPthr_t* x)
{
  x->is_working = false;
  x->num_locks  = 0;
  x->cond_lock  = 0;

  hpcrun_atomicDecr(x->ps_num_threads);
  
  hpcrun_atomicDecr(x->ps_num_working);
  // x->ps_num_working_lock: same

  // x->ps_num_idle_cond: same
}


static inline void
lushPthr_lock_pre_ty2(lushPthr_t* x)
{
  hpcrun_atomicDecr(x->ps_num_working);
  // x->ps_num_working_lock: same

  // x->ps_num_idle_cond: same

  x->is_working = false;
  // x->num_locks: same
  // x->cond_lock: same
}


static inline void
lushPthr_lock_post_ty2(lushPthr_t* x)
{
  // (1) moving to lock; (2) moving from cond to lock
  bool do_addLock = (x->num_locks == 0 || lushPthr_isDirectlyInCond(x));

  x->is_working = true;
  x->num_locks++;
  // x->cond_lock: same

  hpcrun_atomicIncr(x->ps_num_working);
  if (do_addLock) {
    hpcrun_atomicIncr(x->ps_num_working_lock);
  }

  // x->ps_num_idle_cond: same
}


static inline void
lushPthr_trylock_ty2(lushPthr_t* x)
{
  // (1) moving to lock; (2) moving from cond to lock
  bool do_addLock = (x->num_locks == 0 || lushPthr_isDirectlyInCond(x));

  x->is_working = true; // same
  x->num_locks++;
  // x->cond_lock: same

  // x->ps_num_working: // same
  if (do_addLock) {
    hpcrun_atomicIncr(x->ps_num_working_lock);
  }

  // x->ps_num_idle_cond: same
}


static inline void
lushPthr_unlock_ty2(lushPthr_t* x)
{
  bool wasDirectlyInCond = lushPthr_isDirectlyInCond(x);

  x->is_working = true; // same
  x->num_locks--;
  if (wasDirectlyInCond) {
    x->cond_lock = 0;
  }
  
  // x->ps_num_working: same
  if ((x->num_locks == 0 && !wasDirectlyInCond) 
      || lushPthr_isDirectlyInCond(x)) {
    hpcrun_atomicDecr(x->ps_num_working_lock);
  }

  // x->ps_num_idle_cond: same
}


static inline void
lushPthr_mutexLock_pre_ty2(lushPthr_t* restrict x, 
			   pthread_mutex_t* restrict lock)
{
  lushPthr_lock_pre_ty2(x);
}


static inline void
lushPthr_mutexLock_post_ty2(lushPthr_t* restrict x, 
			    pthread_mutex_t* restrict lock)
{
  lushPthr_lock_post_ty2(x);
}


static inline void
lushPthr_mutexTrylock_post_ty2(lushPthr_t* restrict x, 
			       pthread_mutex_t* restrict lock)
{
  lushPthr_trylock_ty2(x);
}


static inline void
lushPthr_mutexUnlock_post_ty2(lushPthr_t* restrict x, 
			      pthread_mutex_t* restrict lock)
{
  lushPthr_unlock_ty2(x);
}


static inline pthread_spinlock_t*
lushPthr_spinLock_pre_ty2(lushPthr_t* restrict x, 
			  pthread_spinlock_t* restrict lock)
{
  lushPthr_lock_pre_ty2(x);
  return lock;
}


static inline void
lushPthr_spinLock_post_ty2(lushPthr_t* restrict x, 
			   pthread_spinlock_t* restrict lock)
{
  lushPthr_lock_post_ty2(x);
}


static inline pthread_spinlock_t*
lushPthr_spinTrylock_pre_ty2(lushPthr_t* restrict x, 
			     pthread_spinlock_t* restrict lock)
{
  return lock;
}


static inline void
lushPthr_spinTrylock_post_ty2(lushPthr_t* restrict x, 
			      pthread_spinlock_t* restrict lock)
{
  lushPthr_trylock_ty2(x);
}


static inline pthread_spinlock_t*
lushPthr_spinUnlock_pre_ty2(lushPthr_t* restrict x, 
			    pthread_spinlock_t* restrict lock)
{
  return lock;
}


static inline void
lushPthr_spinUnlock_post_ty2(lushPthr_t* restrict x, 
			     pthread_spinlock_t* restrict lock)
{
  lushPthr_unlock_ty2(x);
}


static inline pthread_spinlock_t*
lushPthr_spinDestroy_pre_ty2(lushPthr_t* restrict x, 
			     pthread_spinlock_t* restrict lock)
{
  return lock;
}


static inline void
lushPthr_spinDestroy_post_ty2(lushPthr_t* restrict x, 
			      pthread_spinlock_t* restrict lock)
{
}


static inline void
lushPthr_condwait_pre_ty2(lushPthr_t* x)
{
  bool wasDirectlyInCond = lushPthr_isDirectlyInCond(x);
  int new_num_locks = (x->num_locks - 1);
  
  // N.B. this order ensures that (num_working - num_working_lock) >= 0
  if (new_num_locks == 0 && !wasDirectlyInCond) {
    hpcrun_atomicDecr(x->ps_num_working_lock);
  }
  hpcrun_atomicDecr(x->ps_num_working);

  hpcrun_atomicIncr(x->ps_num_idle_cond);

  x->is_working = false;
  x->num_locks = new_num_locks;
  //x->cond_lock: same
}


static inline void
lushPthr_condwait_post_ty2(lushPthr_t* x)
{
  x->is_working = true;
  x->num_locks++;
  x->cond_lock = x->num_locks;

  hpcrun_atomicIncr(x->ps_num_working);
  // x->ps_num_working_lock: same, b/c thread is part of 'num_working_cond'

  hpcrun_atomicDecr(x->ps_num_idle_cond);
}


//***************************************************************************
// 3. Attribute lock-wait time to the perpetrator.
//***************************************************************************

// N.B. consistent with the pthreads implementation
#define lushPthr_LockValMax (0x0) // locked values: [INT_MIN, 0]
#define lushPthr_UnlckVal   (0x1)
#define lushPthr_DestroyVal (-1)

#define lushPthr_maxValueOfLock (1)

//***************************************************************************

static inline bool
lushPthr_isSyncDataPointer(pthread_spinlock_t lockval)
{
  return (lockval > lushPthr_maxValueOfLock);
}


static inline lushPtr_SyncObjData_t*
lushPthr_getSyncDataPointer(pthread_spinlock_t lockval)
{
  return (lushPtr_SyncObjData_t*)(lushPthr_mem_beg + lockval);
}


static inline int32_t
lushPthr_makeSyncDataPointer(lushPtr_SyncObjData_t* data)
{
  return (intptr_t)((void*)data - lushPthr_mem_beg);
}


static inline void
lushPthr_destroySyncDataPointer(pthread_spinlock_t* lock)
{
  *lock = lushPthr_DestroyVal;
}


// lushPthr_freelstEnq: push x on the tail
static inline void
lushPthr_freelstEnq(lushPthr_t* restrict pthr, 
		    lushPtr_SyncObjData_t* restrict x)
{
  x->next = NULL;
  if (!pthr->freelstTail) {
    // Case 1: empty
    pthr->freelstHead = x;
    pthr->freelstTail = x;
  }
  else {
    // Case 2: non-empty
    pthr->freelstTail->next = x;
    pthr->freelstTail = x;
  }
#if (LUSH_DBG_STATS)
  hpcrun_atomicIncr(&DBG_numLockFreelistCur);
#endif
}


// lushPthr_freelstDeq: pop from the head, if possible
static inline lushPtr_SyncObjData_t*
lushPthr_freelstDeq(lushPthr_t* pthr)
{
  if (!pthr->freelstHead) {
    // Case 1: empty
    return NULL;
  }
  else if (pthr->freelstHead == pthr->freelstHead) {
    // Case 2: non-empty
    lushPtr_SyncObjData_t* x = pthr->freelstHead;
    pthr->freelstHead = x->next;
    x->next = NULL;

    if (!pthr->freelstHead) {
      // Special case: one element
      pthr->freelstTail = NULL;
    }
#if (LUSH_DBG_STATS)
    hpcrun_atomicDecr(&DBG_numLockFreelistCur);
#endif
    
    return x;
  }
}


static inline lushPtr_SyncObjData_t*
lushPthr_makeSyncObjData_spin(lushPthr_t* restrict pthr, 
			      pthread_spinlock_t* restrict lock)
{
  lushPtr_SyncObjData_t* x = lushPthr_freelstDeq(pthr);
  if (!x) {
    x = lushPthr_malloc(sizeof(lushPtr_SyncObjData_t));
  }
  if (!x) {
    assert(0 && "LUSH/Pthreads: exhausted lock memory");
  }
  lushPtr_SyncObjData_init(x); 
#if (LUSH_DBG_STATS)
  hpcrun_atomicIncr(&DBG_numLockAlloc);
  long lockAllocCur = (((lushPthr_mem_ptr - lushPthr_mem_beg)
			/ sizeof(lushPtr_SyncObjData_t))
		       - 1 - DBG_numLockFreelistCur);
  long result;
  read_modify_write(long, &DBG_maxLockAllocCur, 
		    MAX(DBG_maxLockAllocCur, lockAllocCur), result);
#endif
  return x;
}


static inline lushPtr_SyncObjData_t*
lushPthr_demandSyncObjData_spin(lushPthr_t* restrict pthr,
				pthread_spinlock_t* restrict lock)
{
  // test-and-test-and-set
  if (!lushPthr_isSyncDataPointer(*lock)) {
    lushPtr_SyncObjData_t* data = lushPthr_makeSyncObjData_spin(pthr, lock);
    int32_t newval = lushPthr_makeSyncDataPointer(data);

    bool isWinner = false;
    while (true) {
      // CAS returns *old* value iff successful
      int32_t oldval = *lock;
      if (lushPthr_isSyncDataPointer(oldval)) {
	break;
      }
      
      data->lock.spin = oldval;
      isWinner = (compare_and_swap_i32(lock, oldval, newval) == oldval);
      if (isWinner) {
	break;
      }
    }
    
    if (!isWinner) {
      lushPthr_freelstEnq(pthr, data); // enqueue onto free list          
    }
  }
  // INVARIANT: lushPthr_isSyncDataPointer(*lock) is true

  return lushPthr_getSyncDataPointer(*lock);
}


static inline lushPtr_SyncObjData_t*
lushPthr_demandCachedSyncObjData_spin(lushPthr_t* restrict pthr, 
				      pthread_spinlock_t* restrict lock)
{
  if ((void*)lock != pthr->cache_syncObj) {
    pthr->cache_syncObj = (void*)lock;
    pthr->cache_syncObjData = lushPthr_demandSyncObjData_spin(pthr, lock);
  }
  return pthr->cache_syncObjData;
}


static inline lushPtr_SyncObjData_t*
lushPthr_demandSyncObjData_ps(lushPthr_t* restrict x, void* restrict syncObj)
{
  //hpcrun_safe_enter(); // inherited

  BalancedTreeNode_t* fnd = 
    BalancedTree_find(x->ps_syncObjToData, syncObj, &x->locklcl);
  if (!fnd) {
    fnd = BalancedTree_insert(x->ps_syncObjToData, syncObj, &x->locklcl);
    lushPtr_SyncObjData_init(fnd->data);
#if (LUSH_DBG_STATS)
    hpcrun_atomicIncr(&DBG_numLockAlloc);
#endif
  }

  //hpcrun_safe_exit(); // inherited

  return fnd->data;
}


static inline lushPtr_SyncObjData_t*
lushPthr_demandSyncObjData(lushPthr_t* restrict x, void* restrict syncObj)
{
  hpcrun_safe_enter();

  BalancedTreeNode_t* fnd = 
    BalancedTree_find(&x->syncObjToData, syncObj, NULL/*lock*/);
  if (!fnd) {
    fnd = BalancedTree_insert(&x->syncObjToData, syncObj, NULL/*lock*/);
    fnd->data = lushPthr_demandSyncObjData_ps(x, syncObj);
  }

  hpcrun_safe_exit();

  return fnd->data;
}


static inline lushPtr_SyncObjData_t*
lushPthr_demandCachedSyncObjData(lushPthr_t* restrict pthr, 
				 void* restrict syncObj)
{
  if (syncObj != pthr->cache_syncObj) {
    pthr->cache_syncObj = syncObj;
    pthr->cache_syncObjData = lushPthr_demandSyncObjData(pthr, (void*)syncObj);
  }
  return pthr->cache_syncObjData;
}


//***************************************************************************

static inline int
lushPthr_spin_lock(pthread_spinlock_t* lock)
{
  while (true) {
    if (lushPthr_isSyncDataPointer(*lock)) {
      // ------------------------------------------------------------
      // acquire an indirect lock
      // ------------------------------------------------------------
      lushPtr_SyncObjData_t* data = lushPthr_getSyncDataPointer(*lock);
      lock = &data->lock.spin;
      while (true) {
	while (*lock <= lushPthr_LockValMax) {;}
	if (fetch_and_store_i32(lock, lushPthr_LockValMax)
	    == lushPthr_UnlckVal) {
	  return 0; // success
	}
      }
    }
    // ------------------------------------------------------------
    // acquire a direct lock 
    // ------------------------------------------------------------
    while (*lock <= lushPthr_LockValMax) {;}
    if (compare_and_swap_i32(lock, lushPthr_UnlckVal, lushPthr_LockValMax) 
	== lushPthr_UnlckVal) {
      return 0; // success
    }
  }
  return 1;
}


static inline int
lushPthr_spin_trylock(pthread_spinlock_t* lock)
{
  while (true) {
    if (lushPthr_isSyncDataPointer(*lock)) {
      // ------------------------------------------------------------
      // acquire an indirect lock
      // ------------------------------------------------------------
      lushPtr_SyncObjData_t* data = lushPthr_getSyncDataPointer(*lock);
      lock = &data->lock.spin;
      int prev = fetch_and_store_i32(lock, lushPthr_LockValMax);
      return ((prev == lushPthr_UnlckVal) ? 0 /*success*/ : 1);
    }
    // ------------------------------------------------------------
    // acquire a direct lock 
    // ------------------------------------------------------------
    int prev = compare_and_swap_i32(lock, lushPthr_UnlckVal, lushPthr_LockValMax);
    if (prev == lushPthr_UnlckVal) {
      return 0; // success
    }
    else if (prev <= lushPthr_LockValMax) { 
      return 1;
    }
  }
   
}


static inline int
lushPthr_spin_unlock(pthread_spinlock_t* lock)
{
  while (true) {
    int lockval = *lock;
    if (lushPthr_isSyncDataPointer(lockval)) {
      lushPtr_SyncObjData_t* data = lushPthr_getSyncDataPointer(lockval);
      data->lock.spin = lushPthr_UnlckVal;
      return 0; // success
    }
    
    if (compare_and_swap_i32(lock, lockval, lushPthr_UnlckVal) == lockval) {
      return 0; // success
    }
  }
  return 1;
}


//***************************************************************************

static inline void
lushPthr_thread_init_ty3(lushPthr_t* x)
{
  x->is_working = true;
}


static inline void
lushPthr_thread_fini_ty3(lushPthr_t* x)
{
  x->is_working = false;
}


static inline void
lushPthr_mutexLock_pre_ty3(lushPthr_t* restrict x, 
			   pthread_mutex_t* restrict lock)
{
  lushPtr_SyncObjData_t* syncData = 
    lushPthr_demandCachedSyncObjData(x, (void*)lock);
  syncData->isBlockingWork = (syncData->isLocked);

#if (LUSH_DBG_STATS)
  hpcrun_atomicIncr(&DBG_numLockAcq);
#endif

  lushPthr_begSmplIdleness(x);
  x->syncObjData = NULL; // drop samples
  x->is_working = false;
}


static inline void
lushPthr_mutexLock_post_ty3(lushPthr_t* restrict x, 
			    pthread_mutex_t* restrict lock)
{
  x->is_working = true;
  lushPthr_endSmplIdleness(x);

  lushPtr_SyncObjData_t* syncData = 
    lushPthr_demandCachedSyncObjData(x, (void*)lock);
  syncData->isLocked = true;

  if (x->idleness > 0 && syncData->cct_node) {
    cct_node_t* node = (cct_node_t*)syncData->cct_node;
    int mid = lush_agents->metric_idleness;
    double idleness = x->idleness;
    cct_metric_data_increment(mid,
			      node,
			      (cct_metric_data_t){.r = idleness});
  }
}


static inline void
lushPthr_mutexTrylock_post_ty3(lushPthr_t* restrict x, 
			       pthread_mutex_t* restrict lock)
{
  x->is_working = true; // same
}


static inline void
lushPthr_mutexUnlock_post_ty3(lushPthr_t* restrict x, 
			      pthread_mutex_t* restrict lock)
{
  x->is_working = true; // same
  
  lushPtr_SyncObjData_t* syncData = 
    lushPthr_demandCachedSyncObjData(x, (void*)lock);
  syncData->isLocked = false;

  if (syncData->isBlockingWork) {
    // FIXME3: may not be blocking and may do more work than necessary
    syncData->cct_node = lushPthr_attribToCallPath(0);
  }
}


static inline pthread_spinlock_t*
lushPthr_spinLock_pre_ty3(lushPthr_t* restrict x, 
			  pthread_spinlock_t* restrict lock)
{
  lushPtr_SyncObjData_t* syncData = 
    lushPthr_demandCachedSyncObjData_spin(x, lock);

#if (LUSH_DBG_STATS)
  hpcrun_atomicIncr(&DBG_numLockAcq);
#endif

  x->syncObjData = syncData;
  x->is_working = false;
  return &syncData->lock.spin;
}


static inline void
lushPthr_spinLock_post_ty3(lushPthr_t* restrict x, 
			   pthread_spinlock_t* restrict lock)
{
  x->is_working = true;
  x->syncObjData = NULL;
}


static inline pthread_spinlock_t*
lushPthr_spinTrylock_pre_ty3(lushPthr_t* restrict x, 
			     pthread_spinlock_t* restrict lock)
{
  lushPtr_SyncObjData_t* syncData = 
    lushPthr_demandCachedSyncObjData_spin(x, lock);
  return &syncData->lock.spin;
}


static inline void
lushPthr_spinTrylock_post_ty3(lushPthr_t* restrict x, 
			      pthread_spinlock_t* restrict lock)
{
  x->is_working = true; // same
}


static inline pthread_spinlock_t*
lushPthr_spinUnlock_pre_ty3(lushPthr_t* restrict x, 
			    pthread_spinlock_t* restrict lock)
{
  lushPtr_SyncObjData_t* syncData = 
    lushPthr_demandCachedSyncObjData_spin(x, lock);
  return &syncData->lock.spin;
}


static inline void
lushPthr_spinUnlock_post_ty3(lushPthr_t* restrict x, 
			     pthread_spinlock_t* restrict lock)
{
  x->is_working = true; // same
  
  lushPtr_SyncObjData_t* syncData = 
    lushPthr_demandCachedSyncObjData_spin(x, lock);
  if (syncData && syncData->idleness > 0) {
    x->idleness = fetch_and_store_i64(&(syncData->idleness), 0);
    lushPthr_attribToCallPath(x->idleness);
  }
}


static inline pthread_spinlock_t*
lushPthr_spinDestroy_pre_ty3(lushPthr_t* restrict x, 
			     pthread_spinlock_t* restrict lock)
{
  pthread_spinlock_t* real_lock = lock;
  if (lushPthr_isSyncDataPointer(*lock)) {
    lushPtr_SyncObjData_t* syncData = lushPthr_getSyncDataPointer(*lock);
    real_lock = &syncData->lock.spin;
  }
  return real_lock;
}


static inline void
lushPthr_spinDestroy_post_ty3(lushPthr_t* restrict x, 
			      pthread_spinlock_t* restrict lock)
{
  if (lushPthr_isSyncDataPointer(*lock)) {
    lushPtr_SyncObjData_t* syncData = lushPthr_getSyncDataPointer(*lock);
    lushPthr_freelstEnq(x, syncData); // enqueue onto free list
    lushPthr_destroySyncDataPointer(lock);
  }
}


static inline void
lushPthr_condwait_pre_ty3(lushPthr_t* x)
{
  x->is_working = false;
  //FIXME3: lushPthr_begSmplIdleness(x);
}


static inline void
lushPthr_condwait_post_ty3(lushPthr_t* x)
{
  //FIXME3: lushPthr_endSmplIdleness(x);
  x->is_working = true;
}


//***************************************************************************
// 
//***************************************************************************

// create (thread): 
static inline void
lushPthr_thread_init(lushPthr_t* x)
{
  if (LUSH_PTHR_DBG) { lushPthr_dump(x, "thrInit", NULL); }

  LUSH_PTHR_FN(lushPthr_thread_init)(x);
}


// destroy (thread): 
static inline void
lushPthr_thread_fini(lushPthr_t* x)
{
  if (LUSH_PTHR_DBG) { lushPthr_dump(x, "thrFini", NULL); }

  LUSH_PTHR_FN(lushPthr_thread_fini)(x);
}


// ---------------------------------------------------------
// mutex_lock
// ---------------------------------------------------------

// lock_pre: thread blocks/sleeps
static inline void
lushPthr_mutexLock_pre(lushPthr_t* restrict x, pthread_mutex_t* restrict lock)
{
  if (LUSH_PTHR_DBG) { lushPthr_dump(x, "mLock[", NULL); }

  LUSH_PTHR_FN(lushPthr_mutexLock_pre)(x, lock);
}


// lock_post: thread acquires lock and continues
static inline void
lushPthr_mutexLock_post(lushPthr_t* restrict x, pthread_mutex_t* restrict lock)
{
  if (LUSH_PTHR_DBG) { lushPthr_dump(x, "mLock]", NULL); }

  LUSH_PTHR_FN(lushPthr_mutexLock_post)(x, lock);
}


// trylock: thread may acquire lock, but always continues (never blocks)
static inline void
lushPthr_mutexTrylock_post(lushPthr_t* restrict x, 
			   pthread_mutex_t* restrict lock,
			   int result)
{
  if (result != 0) {
    return; // lock was not acquired -- epoch remains the same
  }

  if (LUSH_PTHR_DBG) { lushPthr_dump(x, "mTrylock", NULL); }

  LUSH_PTHR_FN(lushPthr_mutexTrylock_post)(x, lock);
}


// unlock: thread releases lock and continues
static inline void
lushPthr_mutexUnlock_post(lushPthr_t* restrict x, 
			  pthread_mutex_t* restrict lock)
{
  if (LUSH_PTHR_DBG) { lushPthr_dump(x, "mUnlock", NULL); }

  LUSH_PTHR_FN(lushPthr_mutexUnlock_post)(x, lock);
}


// ---------------------------------------------------------
// spin_wait
// ---------------------------------------------------------

// lock_pre: thread blocks/sleeps
static inline pthread_spinlock_t*
lushPthr_spinLock_pre(lushPthr_t* restrict x, 
		      pthread_spinlock_t* lock)
{
  if (LUSH_PTHR_DBG) { lushPthr_dump(x, "sLock[", (void*)lock); }

  return LUSH_PTHR_FN(lushPthr_spinLock_pre)(x, lock);
}


// lock_post: thread acquires lock and continues
static inline void
lushPthr_spinLock_post(lushPthr_t* restrict x, 
		       pthread_spinlock_t* restrict lock)
{
  if (LUSH_PTHR_DBG) { lushPthr_dump(x, "sLock]", (void*)lock); }

  LUSH_PTHR_FN(lushPthr_spinLock_post)(x, lock);
}


// trylock_pre: 
static inline pthread_spinlock_t*
lushPthr_spinTrylock_pre(lushPthr_t* restrict x, 
			 pthread_spinlock_t* restrict lock)
{
  if (LUSH_PTHR_DBG) { lushPthr_dump(x, "sTrylock[", (void*)lock); }

  return LUSH_PTHR_FN(lushPthr_spinTrylock_pre)(x, lock);
}


// trylock_post: thread may acquire lock, but always continues (never blocks)
static inline void
lushPthr_spinTrylock_post(lushPthr_t* restrict x, 
			  pthread_spinlock_t* restrict lock,
			  int result)
{
  if (result != 0) {
    return; // lock was not acquired -- epoch remains the same
  }

  if (LUSH_PTHR_DBG) { lushPthr_dump(x, "sTrylock]", (void*)lock); }

  LUSH_PTHR_FN(lushPthr_spinTrylock_post)(x, lock);
}


// unlock_pre: 
static inline pthread_spinlock_t*
lushPthr_spinUnlock_pre(lushPthr_t* restrict x, 
			pthread_spinlock_t* restrict lock)
{
  if (LUSH_PTHR_DBG) { lushPthr_dump(x, "sUnlock[", (void*)lock); }

  return LUSH_PTHR_FN(lushPthr_spinUnlock_pre)(x, lock);
}


// unlock_post: thread releases lock and continues
static inline void
lushPthr_spinUnlock_post(lushPthr_t* restrict x, 
			 pthread_spinlock_t* restrict lock)
{
  if (LUSH_PTHR_DBG) { lushPthr_dump(x, "sUnlock]", (void*)lock); }

  LUSH_PTHR_FN(lushPthr_spinUnlock_post)(x, lock);
}


static inline pthread_spinlock_t*
lushPthr_spinDestroy_pre(lushPthr_t* restrict x, 
			 pthread_spinlock_t* restrict lock)
{
  if (LUSH_PTHR_DBG) { lushPthr_dump(x, "sDstroy[", (void*)lock); }

  return LUSH_PTHR_FN(lushPthr_spinDestroy_pre)(x, lock);
}


static inline void
lushPthr_spinDestroy_post(lushPthr_t* restrict x, 
			  pthread_spinlock_t* restrict lock)
{
  if (LUSH_PTHR_DBG) { lushPthr_dump(x, "sDstroy]", (void*)lock); }

  LUSH_PTHR_FN(lushPthr_spinDestroy_post)(x, lock);
}


// ---------------------------------------------------------
// cond_wait
// ---------------------------------------------------------

// condwait_pre: associated lock is released and thread blocks
static inline void
lushPthr_condwait_pre(lushPthr_t* x)
{
  if (LUSH_PTHR_DBG) { lushPthr_dump(x, "cwait[", NULL); }

  LUSH_PTHR_FN(lushPthr_condwait_pre)(x);
}


// condwait_post: associated lock is acquired and thread continues
static inline void
lushPthr_condwait_post(lushPthr_t* x)
{
  if (LUSH_PTHR_DBG) { lushPthr_dump(x, "cwait]", NULL); }

  LUSH_PTHR_FN(lushPthr_condwait_post)(x);
}


// **************************************************************************

#if defined(__cplusplus)
} /* extern "C" */
#endif


#endif /* lush_pthreads_h */
