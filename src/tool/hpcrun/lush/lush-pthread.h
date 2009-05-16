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

#include "atomic.h"

//*************************** Forward Declarations **************************

#define LUSH_PTHR_DBG 0

// FIXME: this should be promoted to the official atomic.h
#if defined(__GNUC__)
#  define GCC_VERSION (__GNUC__*1000 + __GNUC_MINOR__*100 + __GNUC_PATCHLEVEL__)
#else
#  define GCC_VERSION 0
#endif

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


// ---------------------------------------------------------
// 
// ---------------------------------------------------------

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


typedef struct {
  
  // -------------------------------------------------------
  // thread specific metrics
  // -------------------------------------------------------
  bool is_working; // thread is working (not blocked for any reason)
  int  num_locks;  // number of thread's locks (including a cond-var lock)
  int  cond_lock;  // 0 or the lock number on entry to cond-var critial region

  // -------------------------------------------------------
  // process wide metrics
  // -------------------------------------------------------
  long* ps_num_procs;        // available processor cores
  long* ps_num_threads;

  long* ps_num_working;      // working (1) + (2) + (3)
  long* ps_num_working_lock; // working (1)
  //    ps_num_working_othr  // working (2) + (3)

  //    ps_num_idle_lock     // idleness [1]
  long* ps_num_idle_cond;    // idleness [2a] + [2b]

} lush_pthr_t;


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

void 
lush_pthr__init(lush_pthr_t* x);

void 
lush_pthr__dump(lush_pthr_t* x, const char* nm);


//***************************************************************************

// create (thread): 
static inline void
lush_pthr__thread_init(lush_pthr_t* x)
{
  x->is_working = true;
  x->num_locks  = 0;
  x->cond_lock  = 0;
  
  MY_atomic_increment(x->ps_num_threads);

  MY_atomic_increment(x->ps_num_working);
  // x->ps_num_working_lock: same

  // x->ps_num_idle_cond: same

  if (LUSH_PTHR_DBG) { lush_pthr__dump(x, "t_init"); }
}


// destroy (thread): 
static inline void
lush_pthr__thread_fini(lush_pthr_t* x)
{
  x->is_working = false;
  x->num_locks  = 0;
  x->cond_lock  = 0;

  MY_atomic_decrement(x->ps_num_threads);
  
  MY_atomic_decrement(x->ps_num_working);
  // x->ps_num_working_lock: same

  // x->ps_num_idle_cond: same

  if (LUSH_PTHR_DBG) { lush_pthr__dump(x, "t_fini"); }
}


//***************************************************************************

// lock_pre: thread blocks
static inline void
lush_pthr__lock_pre(lush_pthr_t* x)
{
  MY_atomic_decrement(x->ps_num_working);
  // x->ps_num_working_lock: same

  // x->ps_num_idle_cond: same

  x->is_working = false;
  // x->num_locks: same
  // x->cond_lock: same

  if (LUSH_PTHR_DBG) { lush_pthr__dump(x, "lock["); }
}


// lock_post: thread acquires lock and continues
static inline void
lush_pthr__lock_post(lush_pthr_t* x)
{
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

  if (LUSH_PTHR_DBG) { lush_pthr__dump(x, "lock]"); }
}


// trylock: thread may acquire lock, but always continues (never blocks)
static inline void
lush_pthr__trylock(lush_pthr_t* x, int result)
{
  if (result != 0) {
    return; // lock was not acquired -- state remains the same
  }

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

  if (LUSH_PTHR_DBG) { lush_pthr__dump(x, "trylock"); }
}


// unlock: thread releases lock and continues
static inline void
lush_pthr__unlock(lush_pthr_t* x)
{
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

  if (LUSH_PTHR_DBG) { lush_pthr__dump(x, "unlock"); }
}


//***************************************************************************

// condwait_pre: associated lock is released and thread blocks
static inline void
lush_pthr__condwait_pre(lush_pthr_t* x)
{
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

  if (LUSH_PTHR_DBG) { lush_pthr__dump(x, "cwait["); }
}


// condwait_post: associated lock is acquired and thread continues
static inline void
lush_pthr__condwait_post(lush_pthr_t* x)
{
  x->is_working = true;
  x->num_locks++;
  x->cond_lock = x->num_locks;

  MY_atomic_increment(x->ps_num_working);
  // x->ps_num_working_lock: same, b/c thread is part of 'num_working_cond'

  MY_atomic_decrement(x->ps_num_idle_cond);

  if (LUSH_PTHR_DBG) { lush_pthr__dump(x, "cwait]"); }
}


// **************************************************************************

#if defined(__cplusplus)
} /* extern "C" */
#endif

#undef MY_atomic_increment
#undef MY_atomic_decrement


#endif /* lush_pthreads_h */
