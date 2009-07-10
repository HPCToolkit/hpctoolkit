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

  long* ps_num_working;      // working (1) + (2) + (3)
  long* ps_num_working_lock; // working (1)
  //    ps_num_working_othr  // working (2) + (3)

  //    ps_num_idle_lock     // idleness [1]
  long* ps_num_idle_cond;    // idleness [2a] + [2b]

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
