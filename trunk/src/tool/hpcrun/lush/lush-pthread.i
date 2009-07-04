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

#include <utilities/BalancedTree.h>

#include <lib/prof-lean/QueuingRWLock.h>

//*************************** Forward Declarations **************************

//***************************************************************************

#if defined(__cplusplus)
extern "C" {
#endif

//***************************************************************************

typedef struct SyncObjData
{
  uint64_t idleness;
  void*    cct_node;

} SyncObjData_t;


//***************************************************************************
// 
//***************************************************************************

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

  // -------------------------------------------------------
  // LUSH_PTHR_FN_TY == 3
  // -------------------------------------------------------
  BalancedTree_t* syncObjToData; // synch-obj -> data
  BalancedTreeNode_t* syncObjData;
  QueuingRWLockLcl_t locklcl;

  void*               cache_syncObj;
  BalancedTreeNode_t* cache_syncObjData;

} lushPthr_t;


// **************************************************************************

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* lush_pthreads_i */
