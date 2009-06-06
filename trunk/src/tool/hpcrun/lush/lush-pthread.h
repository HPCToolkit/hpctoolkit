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

#define LUSH_PTHR_SELF_IDLENESS    1  /* if 0, attrib idleness to workers */
#define LUSH_PTHR_SYNC_SMPL_PERIOD 83 /* if !0, sample synchronously */

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


//***************************************************************************

void 
lush_pthr__init(lush_pthr_t* x);

void 
lush_pthr__dump(lush_pthr_t* x, const char* nm);


//***************************************************************************

static inline bool
lush_pthr__isDirectlyInCond(lush_pthr_t* x)
{
  return (x->cond_lock != 0 && x->cond_lock == x->num_locks);
}


static inline bool
lush_pthr__isWorking_lock(lush_pthr_t* x)
{
  return (x->is_working && x->num_locks > 0 && !lush_pthr__isDirectlyInCond(x));
}


static inline bool
lush_pthr__isWorking_cond(lush_pthr_t* x)
{
  return (x->is_working && lush_pthr__isDirectlyInCond(x));
}


//***************************************************************************


static inline void
lush_pthr__begSmplIdleness(lush_pthr_t* x)
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
lush_pthr__endSmplIdleness(lush_pthr_t* x)
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


#if 1
#  define lush_pthr__commitSmplIdleness(/* lush_pthr_t* */ x)	   \
  if (x->idleness > 0 && !hpcrun_async_is_blocked()) {		   \
    hpcrun_async_block();					   \
    ucontext_t context;						   \
    getcontext(&context); /* FIXME: check for errors */		   \
    hpcrun_sample_callpath(&context, lush_agents->metric_time,		  \
			   0/*metricIncr*/, 0/*skipInner*/, 1/*isSync*/); \
    hpcrun_async_unblock();						\
  }
#elif 0
  // noticably more overhead and won't work with PAPI
#  include <signal.h>
#  include <pthread.h>
#  define lush_pthr__commitSmplIdleness(/* lush_pthr_t* */ x)	   \
  if (x->idleness > 0) {					   \
    pthread_kill(pthread_self(), SIGPROF);			   \
  }
#else 
#  define lush_pthr__commitSmplIdleness(/* lush_pthr_t* */ x)
#endif

//***************************************************************************

// create (thread): 
static inline void
lush_pthr__thread_init(lush_pthr_t* x)
{
#if (LUSH_PTHR_SELF_IDLENESS)
  x->is_working = true;
#else
  x->is_working = true;
  x->num_locks  = 0;
  x->cond_lock  = 0;

  MY_atomic_increment(x->ps_num_threads);

  MY_atomic_increment(x->ps_num_working);
  // x->ps_num_working_lock: same

  // x->ps_num_idle_cond: same
#endif

  if (LUSH_PTHR_DBG) { lush_pthr__dump(x, "t_init"); }
}


// destroy (thread): 
static inline void
lush_pthr__thread_fini(lush_pthr_t* x)
{
#if (LUSH_PTHR_SELF_IDLENESS)
  x->is_working = false;
#else
  x->is_working = false;
  x->num_locks  = 0;
  x->cond_lock  = 0;

  MY_atomic_decrement(x->ps_num_threads);
  
  MY_atomic_decrement(x->ps_num_working);
  // x->ps_num_working_lock: same

  // x->ps_num_idle_cond: same
#endif

  if (LUSH_PTHR_DBG) { lush_pthr__dump(x, "t_fini"); }
}


//***************************************************************************

// lock_pre: thread blocks
static inline void
lush_pthr__lock_pre(lush_pthr_t* x)
{
#if (LUSH_PTHR_SELF_IDLENESS)
  x->is_working = false;
  lush_pthr__begSmplIdleness(x);
#else
  MY_atomic_decrement(x->ps_num_working);
  // x->ps_num_working_lock: same

  // x->ps_num_idle_cond: same

  x->is_working = false;
  // x->num_locks: same
  // x->cond_lock: same
#endif

  if (LUSH_PTHR_DBG) { lush_pthr__dump(x, "lock["); }
}


// lock_post: thread acquires lock and continues
static inline void
lush_pthr__lock_post(lush_pthr_t* x)
{
#if (LUSH_PTHR_SELF_IDLENESS)
  lush_pthr__endSmplIdleness(x);
  x->is_working = true;
  lush_pthr__commitSmplIdleness(x);
#else
  // (1) moving to lock; (2) moving from cond to lock
  bool do_addLock = (x->num_locks == 0 || lush_pthr__isDirectlyInCond(x));

  x->is_working = true;
  x->num_locks++;
  // x->cond_lock: same

  MY_atomic_increment(x->ps_num_working);
  if (do_addLock) {
    MY_atomic_increment(x->ps_num_working_lock);
  }

  // x->ps_num_idle_cond: same
#endif

  if (LUSH_PTHR_DBG) { lush_pthr__dump(x, "lock]"); }
}


// trylock: thread may acquire lock, but always continues (never blocks)
static inline void
lush_pthr__trylock(lush_pthr_t* x, int result)
{
  if (result != 0) {
    return; // lock was not acquired -- state remains the same
  }

#if (LUSH_PTHR_SELF_IDLENESS)
  x->is_working = true; // same
#else
  // (1) moving to lock; (2) moving from cond to lock
  bool do_addLock = (x->num_locks == 0 || lush_pthr__isDirectlyInCond(x));

  x->is_working = true; // same
  x->num_locks++;
  // x->cond_lock: same

  // x->ps_num_working: // same
  if (do_addLock) {
    MY_atomic_increment(x->ps_num_working_lock);
  }

  // x->ps_num_idle_cond: same
#endif

  if (LUSH_PTHR_DBG) { lush_pthr__dump(x, "trylock"); }
}


// unlock: thread releases lock and continues
static inline void
lush_pthr__unlock(lush_pthr_t* x)
{
#if (LUSH_PTHR_SELF_IDLENESS)
  x->is_working = true; // same
#else
  bool wasDirectlyInCond = lush_pthr__isDirectlyInCond(x);

  x->is_working = true; // same
  x->num_locks--;
  if (wasDirectlyInCond) {
    x->cond_lock = 0;
  }
  
  // x->ps_num_working: same
  if ((x->num_locks == 0 && !wasDirectlyInCond) 
      || lush_pthr__isDirectlyInCond(x)) {
    MY_atomic_decrement(x->ps_num_working_lock);
  }

  // x->ps_num_idle_cond: same
#endif

  if (LUSH_PTHR_DBG) { lush_pthr__dump(x, "unlock"); }
}


//***************************************************************************

// condwait_pre: associated lock is released and thread blocks
static inline void
lush_pthr__condwait_pre(lush_pthr_t* x)
{
#if (LUSH_PTHR_SELF_IDLENESS)
  x->is_working = false;
  lush_pthr__begSmplIdleness(x);
#else
  bool wasDirectlyInCond = lush_pthr__isDirectlyInCond(x);
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
#endif

  if (LUSH_PTHR_DBG) { lush_pthr__dump(x, "cwait["); }
}


// condwait_post: associated lock is acquired and thread continues
static inline void
lush_pthr__condwait_post(lush_pthr_t* x)
{
#if (LUSH_PTHR_SELF_IDLENESS)
  lush_pthr__endSmplIdleness(x);
  x->is_working = true;
  lush_pthr__commitSmplIdleness(x);
#else
  x->is_working = true;
  x->num_locks++;
  x->cond_lock = x->num_locks;

  MY_atomic_increment(x->ps_num_working);
  // x->ps_num_working_lock: same, b/c thread is part of 'num_working_cond'

  MY_atomic_decrement(x->ps_num_idle_cond);
#endif

  if (LUSH_PTHR_DBG) { lush_pthr__dump(x, "cwait]"); }
}


// **************************************************************************

#if defined(__cplusplus)
} /* extern "C" */
#endif

#undef MY_atomic_increment
#undef MY_atomic_decrement


#endif /* lush_pthreads_h */
