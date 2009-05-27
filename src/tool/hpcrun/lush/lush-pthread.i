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

#ifndef lush_pthreads_i
#define lush_pthreads_i

//************************* System Include Files ****************************

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

//*************************** User Include Files ****************************

//*************************** Forward Declarations **************************


//*************************** Forward Declarations **************************

#if defined(__cplusplus)
extern "C" {
#endif

//***************************************************************************
// 
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


typedef struct {
  
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

  long* ps_num_working;      // working (1) + (2) + (3)
  long* ps_num_working_lock; // working (1)
  //    ps_num_working_othr  // working (2) + (3)

  //    ps_num_idle_lock     // idleness [1]
  long* ps_num_idle_cond;    // idleness [2a] + [2b]

} lush_pthr_t;


// **************************************************************************

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* lush_pthreads_i */
