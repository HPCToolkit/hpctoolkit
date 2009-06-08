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

//*************************** User Include Files ****************************

#include <include/gcc-attr.h>
#include <include/min-max.h>

#include "lush-pthread.i"
#include "lush-backtrace.h" // for 'lush_agents'

#include <sample_event.h>
#include <utilities/atomic.h>

#include <lib/prof-lean/timer.h>


//*************************** Forward Declarations **************************

#define LUSH_PTHR_FN_TY 1

#define LUSH_PTHR_DBG 0

//*************************** Forward Declarations **************************

// FIXME: this should be promoted to the official atomic.h
#if (GCC_VERSION >= 4100)
#  define MY_atomic_increment(x) (void)__sync_add_and_fetch(x, 1)
#  define MY_atomic_decrement(x) (void)__sync_sub_and_fetch(x, 1)
#else
#  warning "lush-pthread.h: using slow atomics!"
#  define MY_atomic_increment(x) csprof_atomic_increment(x)
#  define MY_atomic_decrement(x) csprof_atomic_decrement(x)
#endif


#if defined(__cplusplus)
extern "C" {
#endif

//***************************************************************************
// 
//***************************************************************************

void 
lush_pthreads__init();

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


static inline void
lushPthr_sampleIdleness(uint64_t idlenessIncr)
{
#if 1
  if (!hpcrun_async_is_blocked()) {
    hpcrun_async_block();
    ucontext_t context;
    getcontext(&context); /* FIXME: check for errors */
    hpcrun_sample_callpath(&context, lush_agents->metric_time,
			   0/*metricIncr*/, 0/*skipInner*/, 1/*isSync*/);
    hpcrun_async_unblock();
  }
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
lushPthr_mutexLock_pre_ty1(lushPthr_t* x, pthread_mutex_t* lock)
{
  x->is_working = false;
  lushPthr_begSmplIdleness(x);
}


static inline void
lushPthr_mutexLock_post_ty1(lushPthr_t* x, pthread_mutex_t* lock)
{
  lushPthr_endSmplIdleness(x);
  x->is_working = true;
  if (x->idleness > 0) {
    lushPthr_sampleIdleness(x->idleness);
  }
}


static inline void
lushPthr_mutexTrylock_ty1(lushPthr_t* x, pthread_mutex_t* lock)
{
  x->is_working = true; // same
}


static inline void
lushPthr_mutexUnlock_ty1(lushPthr_t* x, pthread_mutex_t* lock)
{
  x->is_working = true; // same
}


static inline void
lushPthr_spinLock_pre_ty1(lushPthr_t* x, pthread_spinlock_t* lock)
{
  x->is_working = false;
}


static inline void
lushPthr_spinLock_post_ty1(lushPthr_t* x, pthread_spinlock_t* lock)
{
  x->is_working = true;
}


static inline void
lushPthr_spinTrylock_ty1(lushPthr_t* x, pthread_spinlock_t* lock)
{
  x->is_working = true; // same
}


static inline void
lushPthr_spinUnlock_ty1(lushPthr_t* x, pthread_spinlock_t* lock)
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
    lushPthr_sampleIdleness(x->idleness);
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

  MY_atomic_increment(x->ps_num_threads);

  MY_atomic_increment(x->ps_num_working);
  // x->ps_num_working_lock: same

  // x->ps_num_idle_cond: same
}


static inline void
lushPthr_thread_fini_ty2(lushPthr_t* x)
{
  x->is_working = false;
  x->num_locks  = 0;
  x->cond_lock  = 0;

  MY_atomic_decrement(x->ps_num_threads);
  
  MY_atomic_decrement(x->ps_num_working);
  // x->ps_num_working_lock: same

  // x->ps_num_idle_cond: same
}


static inline void
lushPthr_lock_pre_ty2(lushPthr_t* x)
{
  MY_atomic_decrement(x->ps_num_working);
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

  MY_atomic_increment(x->ps_num_working);
  if (do_addLock) {
    MY_atomic_increment(x->ps_num_working_lock);
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
    MY_atomic_increment(x->ps_num_working_lock);
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
    MY_atomic_decrement(x->ps_num_working_lock);
  }

  // x->ps_num_idle_cond: same
}


static inline void
lushPthr_mutexLock_pre_ty2(lushPthr_t* x, pthread_mutex_t* lock)
{
  lushPthr_lock_pre_ty2(x);
}


static inline void
lushPthr_mutexLock_post_ty2(lushPthr_t* x, pthread_mutex_t* lock)
{
  lushPthr_lock_post_ty2(x);
}


static inline void
lushPthr_mutexTrylock_ty2(lushPthr_t* x, pthread_mutex_t* lock)
{
  lushPthr_trylock_ty2(x);
}


static inline void
lushPthr_mutexUnlock_ty2(lushPthr_t* x, pthread_mutex_t* lock)
{
  lushPthr_unlock_ty2(x);
}


static inline void
lushPthr_spinLock_pre_ty2(lushPthr_t* x, pthread_spinlock_t* lock)
{
  lushPthr_lock_pre_ty2(x);
}


static inline void
lushPthr_spinLock_post_ty2(lushPthr_t* x, pthread_spinlock_t* lock)
{
  lushPthr_lock_post_ty2(x);
}


static inline void
lushPthr_spinTrylock_ty2(lushPthr_t* x, pthread_spinlock_t* lock)
{
  lushPthr_trylock_ty2(x);
}


static inline void
lushPthr_spinUnlock_ty2(lushPthr_t* x, pthread_spinlock_t* lock)
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
    MY_atomic_decrement(x->ps_num_working_lock);
  }
  MY_atomic_decrement(x->ps_num_working);

  MY_atomic_increment(x->ps_num_idle_cond);

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

  MY_atomic_increment(x->ps_num_working);
  // x->ps_num_working_lock: same, b/c thread is part of 'num_working_cond'

  MY_atomic_decrement(x->ps_num_idle_cond);
}


//***************************************************************************
// 3. Attribute lock-wait time to the working thread.  
//***************************************************************************

// TODO type 1: spin locks do not need to use gettimeofday.

// TODO type 3: attribute lock-wait time to the lock in question; then
// when the working thread unlock, it attributes that idleness to
// itself.  Do not have to maintain coutners, etc.
//   
// Cf. notes in proposal


//***************************************************************************
// 
//***************************************************************************

// create (thread): 
static inline void
lushPthr_thread_init(lushPthr_t* x)
{
  LUSH_PTHR_FN(lushPthr_thread_init)(x);

  if (LUSH_PTHR_DBG) { lushPthr_dump(x, "t_init"); }
}


// destroy (thread): 
static inline void
lushPthr_thread_fini(lushPthr_t* x)
{
  LUSH_PTHR_FN(lushPthr_thread_fini)(x);

  if (LUSH_PTHR_DBG) { lushPthr_dump(x, "t_fini"); }
}


// ---------------------------------------------------------
// mutex_lock
// ---------------------------------------------------------

// lock_pre: thread blocks/sleeps
static inline void
lushPthr_mutexLock_pre(lushPthr_t* x, pthread_mutex_t* lock)
{
  LUSH_PTHR_FN(lushPthr_mutexLock_pre)(x, lock);

  if (LUSH_PTHR_DBG) { lushPthr_dump(x, "mLock["); }
}


// lock_post: thread acquires lock and continues
static inline void
lushPthr_mutexLock_post(lushPthr_t* x, pthread_mutex_t* lock)
{
  LUSH_PTHR_FN(lushPthr_mutexLock_post)(x, lock);

  if (LUSH_PTHR_DBG) { lushPthr_dump(x, "mLock]"); }
}


// trylock: thread may acquire lock, but always continues (never blocks)
static inline void
lushPthr_mutexTrylock(lushPthr_t* x, pthread_mutex_t* lock, int result)
{
  if (result != 0) {
    return; // lock was not acquired -- state remains the same
  }

  LUSH_PTHR_FN(lushPthr_mutexTrylock)(x, lock);

  if (LUSH_PTHR_DBG) { lushPthr_dump(x, "mTrylock"); }
}


// unlock: thread releases lock and continues
static inline void
lushPthr_mutexUnlock(lushPthr_t* x, pthread_mutex_t* lock)
{
  LUSH_PTHR_FN(lushPthr_mutexUnlock)(x, lock);

  if (LUSH_PTHR_DBG) { lushPthr_dump(x, "mUnlock"); }
}


// ---------------------------------------------------------
// spin_wait
// ---------------------------------------------------------

// lock_pre: thread blocks/sleeps
static inline void
lushPthr_spinLock_pre(lushPthr_t* x, pthread_spinlock_t* lock)
{
  LUSH_PTHR_FN(lushPthr_spinLock_pre)(x, lock);

  if (LUSH_PTHR_DBG) { lushPthr_dump(x, "sLock["); }
}


// lock_post: thread acquires lock and continues
static inline void
lushPthr_spinLock_post(lushPthr_t* x, pthread_spinlock_t* lock)
{
  LUSH_PTHR_FN(lushPthr_spinLock_post)(x, lock);

  if (LUSH_PTHR_DBG) { lushPthr_dump(x, "sLock]"); }
}


// trylock: thread may acquire lock, but always continues (never blocks)
static inline void
lushPthr_spinTrylock(lushPthr_t* x, pthread_spinlock_t* lock, int result)
{
  if (result != 0) {
    return; // lock was not acquired -- state remains the same
  }

  LUSH_PTHR_FN(lushPthr_spinTrylock)(x, lock);

  if (LUSH_PTHR_DBG) { lushPthr_dump(x, "sTrylock"); }
}


// unlock: thread releases lock and continues
static inline void
lushPthr_spinUnlock(lushPthr_t* x, pthread_spinlock_t* lock)
{
  LUSH_PTHR_FN(lushPthr_spinUnlock)(x, lock);

  if (LUSH_PTHR_DBG) { lushPthr_dump(x, "sUnlock"); }
}


// ---------------------------------------------------------
// cond_wait
// ---------------------------------------------------------

// condwait_pre: associated lock is released and thread blocks
static inline void
lushPthr_condwait_pre(lushPthr_t* x)
{
  LUSH_PTHR_FN(lushPthr_condwait_pre)(x);

  if (LUSH_PTHR_DBG) { lushPthr_dump(x, "cwait["); }
}


// condwait_post: associated lock is acquired and thread continues
static inline void
lushPthr_condwait_post(lushPthr_t* x)
{
  LUSH_PTHR_FN(lushPthr_condwait_post)(x);

  if (LUSH_PTHR_DBG) { lushPthr_dump(x, "cwait]"); }
}


// **************************************************************************

#if defined(__cplusplus)
} /* extern "C" */
#endif

#undef MY_atomic_increment
#undef MY_atomic_decrement


#endif /* lush_pthreads_h */
