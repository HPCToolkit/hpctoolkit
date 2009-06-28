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

#include <sample_event.h>
#include <cct/cct.h>
#include <utilities/atomic.h>
#include <utilities/BalancedTree.h>

#include <lib/prof-lean/timer.h>


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
lushPthr_dump(lushPthr_t* x, const char* nm);


#define LUSH_PTHR_FN_REAL0(FN, TY) FN ## _ty ## TY
#define LUSH_PTHR_FN_REAL1(FN, TY) LUSH_PTHR_FN_REAL0(FN, TY)
#define LUSH_PTHR_FN(FN)           LUSH_PTHR_FN_REAL1(FN, LUSH_PTHR_FN_TY)

//***************************************************************************
// 1. Attribute a thread's idleness to itself (1st person)
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


static inline csprof_cct_node_t*
lushPthr_attribToCallPath(uint64_t idlenessIncr)
{
#if 1
  csprof_cct_node_t* n = NULL;
  if (!hpcrun_async_is_blocked()) {
    hpcrun_async_block();
    ucontext_t context;
    getcontext(&context); /* FIXME: check for errors */
    n = hpcrun_sample_callpath(&context, lush_agents->metric_time,
			       0/*metricIncr*/, 0/*skipInner*/, 1/*isSync*/);
    hpcrun_async_unblock();
  }
  return n;
#endif

#if 0
  // noticably more overhead and won't work with PAPI
#  include <signal.h>
#  include <pthread.h>
  pthread_kill(pthread_self(), SIGPROF);
#endif
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


static inline void
lushPthr_spinLock_pre_ty1(lushPthr_t* restrict x,
			  pthread_spinlock_t* restrict lock)
{
  x->is_working = false;
}


static inline void
lushPthr_spinLock_post_ty1(lushPthr_t* restrict x,
			   pthread_spinlock_t* restrict lock)
{
  x->is_working = true;
}


static inline void
lushPthr_spinTrylock_post_ty1(lushPthr_t* restrict x, 
			      pthread_spinlock_t* restrict lock)
{
  x->is_working = true; // same
}


static inline void
lushPthr_spinUnlock_post_ty1(lushPthr_t* restrict x, 
			     pthread_spinlock_t* restrict lock)
{
  x->is_working = true; // same
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
// 2. Attribute idleness to working threads (3rd person)
//***************************************************************************

// Working threads are in one of three states
//   (1)  locked && ~directly_in_cond
//   (2)  locked &&  directly_in_cond
//   (3) ~locked
//
//   NOTE: A thread in state (2) could additionally grab a lock
//   unrelated to the cond-var, moving to state (1) until the second
//   lock is released.
//
// Idle threads are in one of three state (mutually exclusive):
//   [1]  blocked on a lock
//   [2a] blocked on a cond-var lock
//   [2b] blocked on a cond-var signal
//   [--] none of the above, but blocking on the scheduler (not
// 	  interesting, assuming all cores are utilized)
//
//   NOTE: A thread blocking in state [1] could be working within a cond (2).

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


static inline void
lushPthr_spinLock_pre_ty2(lushPthr_t* restrict x, 
			  pthread_spinlock_t* restrict lock)
{
  lushPthr_lock_pre_ty2(x);
}


static inline void
lushPthr_spinLock_post_ty2(lushPthr_t* restrict x, 
			   pthread_spinlock_t* restrict lock)
{
  lushPthr_lock_post_ty2(x);
}


static inline void
lushPthr_spinTrylock_post_ty2(lushPthr_t* restrict x, 
			      pthread_spinlock_t* restrict lock)
{
  lushPthr_trylock_ty2(x);
}


static inline void
lushPthr_spinUnlock_post_ty2(lushPthr_t* restrict x, 
			     pthread_spinlock_t* restrict lock)
{
  lushPthr_unlock_ty2(x);
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
// 3. Attribute lock-wait time to the working thread.  
//***************************************************************************

static inline BalancedTreeNode_t*
lushPthr_demandSyncObjData(lushPthr_t* restrict x, void* restrict syncObj)
{
  // FIXME: a potential problem at ???
  // allow multiple finds, but exclusive insert
  //   find()  waits for insert() to complete  => test insert lock
  //   find() !waits for find()   to complete  => natural
  //   insert() waits for find()   to complete => ???
  //   insert   waits for insert() to complete => insert lock
  
  BalancedTreeNode_t* fnd = BalancedTree_find(x->syncObjToData, syncObj);
  if (!fnd) {
    //EMSG("lushPthr/demandSyncObjData: %p", syncObj);
    fnd = BalancedTree_insert(x->syncObjToData, syncObj, true/*atomic*/);
  }
  return fnd;
}


#if 0 // FIXME
static inline SpinLockData_t*
lushPthr_makeSpinLockData(lushPthr_t* restrict x, 
			  pthread_spinlock_t* restrict lock)
{
  // N.B.:
  // - Assume we are only called from pthread_spin_init.
  //   Consequently, we don't need to protect from simultaneous
  //   accesses.  
  // - Assume the lock is already initialized
  // 
  assert( !(lushPthr_spinLockMagicLo <= *lock 
	    && *lock < lushPthr_spinLockMagicHi) );
  assert( (*x->nxtSpinLockIdx) < lushPthr_spinLockDataSZ );
  int fnd_idx = (*x->nxtSpinLockIdx)++;
  int lck_idx = lushPthr_spinLockMagicLo + fnd_idx;
  *lock = lck_idx;
  return &(x->spinLockData[fnd_idx]);
}


static inline SpinLockData_t*
lushPthr_findSpinLockData(lushPthr_t* restrict x, 
			  pthread_spinlock_t* restrict lock)
{
  SpinLockData_t* fnd = NULL;
  if (lushPthr_spinLockMagicLo <= *lock && *lock < lushPthr_spinLockMagicHi) {
    int fnd_idx = *lock - lushPthr_spinLockMagicLo;
    fnd = &(x->spinLockData[fnd_idx]);
  }
  return fnd;
}
#endif


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
  // N.B.: There is a small window where we could be sampled.  Rather
  // than setting an 'ignore-sample' flag, we currently depend upon
  // this happening infrequently.
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

#if 0 // FIXME3
  if (x->idleness > 0) {
    SyncObjData_t* syncData = lushPthr_demandSyncObjData(x, lock);
    if (syncData->cct_node) {
      csprof_cct_node_t* node = (csprof_cct_node_t*)syncData->cct_node;
      int mid = lush_agents->metric_idleness;
      double idleness = x->idleness;
      cct_metric_data_increment(mid, &node->metrics[mid],
				(cct_metric_data_t){.r = idleness});
    }
    // FIXME: otherwise???
  }
#endif
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

  x->syncObjData = NULL; // drop samples
  if (x->doIdlenessCnt == 1 ||
      x->doIdlenessCnt == LUSH_PTHR_SYNC_SMPL_PERIOD/2) {
#if 0 // FIXME3
    BalancedTreeNode_t* syncData = lushPthr_demandSyncObjData(x, lock);
    syncData->cct_node = lushPthr_attribToCallPath(0);
#endif
  }
}


static inline void
lushPthr_spinLock_pre_ty3(lushPthr_t* restrict x, 
			  pthread_spinlock_t* restrict lock)
{
  x->syncObjData = lushPthr_demandSyncObjData(x, (void*)lock);
  x->cache_lock = (void*)lock;
  x->cache_syncObjData = x->syncObjData;
  x->is_working = false;
}


static inline void
lushPthr_spinLock_post_ty3(lushPthr_t* restrict x, 
			   pthread_spinlock_t* restrict lock)
{
  x->is_working = true;
  x->syncObjData = NULL;
}


static inline void
lushPthr_spinTrylock_post_ty3(lushPthr_t* restrict x, 
			      pthread_spinlock_t* restrict lock)
{
  x->is_working = true; // same
}


static inline void
lushPthr_spinUnlock_post_ty3(lushPthr_t* restrict x, 
			     pthread_spinlock_t* restrict lock)
{
  x->is_working = true; // same
  
  BalancedTreeNode_t* syncData = x->cache_syncObjData;
  if (lock != x->cache_lock) {
    syncData = lushPthr_demandSyncObjData(x, (void*)lock);
  }
  if (syncData && syncData->idleness > 0) {
    x->idleness = csprof_atomic_swap_l((long*)&syncData->idleness, 0);
    lushPthr_attribToCallPath(x->idleness);
  }
}


static inline void
lushPthr_condwait_pre_ty3(lushPthr_t* x)
{
  x->is_working = false;
  //FIXME: lushPthr_begSmplIdleness(x);
}


static inline void
lushPthr_condwait_post_ty3(lushPthr_t* x)
{
  //FIXME: lushPthr_endSmplIdleness(x);
  x->is_working = true;
}


//***************************************************************************
// 
//***************************************************************************

// create (thread): 
static inline void
lushPthr_thread_init(lushPthr_t* x)
{
  if (LUSH_PTHR_DBG) { lushPthr_dump(x, "thrInit"); }

  LUSH_PTHR_FN(lushPthr_thread_init)(x);
}


// destroy (thread): 
static inline void
lushPthr_thread_fini(lushPthr_t* x)
{
  if (LUSH_PTHR_DBG) { lushPthr_dump(x, "thrFini"); }

  LUSH_PTHR_FN(lushPthr_thread_fini)(x);
}


// ---------------------------------------------------------
// mutex_lock
// ---------------------------------------------------------

// lock_pre: thread blocks/sleeps
static inline void
lushPthr_mutexLock_pre(lushPthr_t* restrict x, pthread_mutex_t* restrict lock)
{
  if (LUSH_PTHR_DBG) { lushPthr_dump(x, "mLock["); }

  LUSH_PTHR_FN(lushPthr_mutexLock_pre)(x, lock);
}


// lock_post: thread acquires lock and continues
static inline void
lushPthr_mutexLock_post(lushPthr_t* restrict x, pthread_mutex_t* restrict lock)
{
  if (LUSH_PTHR_DBG) { lushPthr_dump(x, "mLock]"); }

  LUSH_PTHR_FN(lushPthr_mutexLock_post)(x, lock);
}


// trylock: thread may acquire lock, but always continues (never blocks)
static inline void
lushPthr_mutexTrylock_post(lushPthr_t* restrict x, 
			   pthread_mutex_t* restrict lock,
			   int result)
{
  if (result != 0) {
    return; // lock was not acquired -- state remains the same
  }

  if (LUSH_PTHR_DBG) { lushPthr_dump(x, "mTrylock"); }

  LUSH_PTHR_FN(lushPthr_mutexTrylock_post)(x, lock);
}


// unlock: thread releases lock and continues
static inline void
lushPthr_mutexUnlock_post(lushPthr_t* restrict x, 
			  pthread_mutex_t* restrict lock)
{
  if (LUSH_PTHR_DBG) { lushPthr_dump(x, "mUnlock"); }

  LUSH_PTHR_FN(lushPthr_mutexUnlock_post)(x, lock);
}


// ---------------------------------------------------------
// spin_wait
// ---------------------------------------------------------

// lock_pre: thread blocks/sleeps
static inline void
lushPthr_spinLock_pre(lushPthr_t* restrict x, 
		      pthread_spinlock_t* restrict lock)
{
  if (LUSH_PTHR_DBG) { lushPthr_dump(x, "sLock["); }

  LUSH_PTHR_FN(lushPthr_spinLock_pre)(x, lock);
}


// lock_post: thread acquires lock and continues
static inline void
lushPthr_spinLock_post(lushPthr_t* restrict x, 
		       pthread_spinlock_t* restrict lock)
{
  if (LUSH_PTHR_DBG) { lushPthr_dump(x, "sLock]"); }

  LUSH_PTHR_FN(lushPthr_spinLock_post)(x, lock);
}


// trylock_post: thread may acquire lock, but always continues (never blocks)
static inline void
lushPthr_spinTrylock_post(lushPthr_t* restrict x, 
			  pthread_spinlock_t* restrict lock,
			  int result)
{
  if (result != 0) {
    return; // lock was not acquired -- state remains the same
  }

  if (LUSH_PTHR_DBG) { lushPthr_dump(x, "sTrylock"); }

  LUSH_PTHR_FN(lushPthr_spinTrylock_post)(x, lock);
}


// unlock_post: thread releases lock and continues
static inline void
lushPthr_spinUnlock_post(lushPthr_t* restrict x, 
			 pthread_spinlock_t* restrict lock)
{
  if (LUSH_PTHR_DBG) { lushPthr_dump(x, "sUnlock"); }

  LUSH_PTHR_FN(lushPthr_spinUnlock_post)(x, lock);
}


// ---------------------------------------------------------
// cond_wait
// ---------------------------------------------------------

// condwait_pre: associated lock is released and thread blocks
static inline void
lushPthr_condwait_pre(lushPthr_t* x)
{
  if (LUSH_PTHR_DBG) { lushPthr_dump(x, "cwait["); }

  LUSH_PTHR_FN(lushPthr_condwait_pre)(x);
}


// condwait_post: associated lock is acquired and thread continues
static inline void
lushPthr_condwait_post(lushPthr_t* x)
{
  if (LUSH_PTHR_DBG) { lushPthr_dump(x, "cwait]"); }

  LUSH_PTHR_FN(lushPthr_condwait_post)(x);
}


// **************************************************************************

#if defined(__cplusplus)
} /* extern "C" */
#endif


#endif /* lush_pthreads_h */
