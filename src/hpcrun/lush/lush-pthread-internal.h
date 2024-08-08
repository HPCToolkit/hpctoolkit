// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

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

#include "../../common/lean/gcc-attr.h"
#include "../../common/lean/BalancedTree.h"

#ifndef __cplusplus
#include <stdatomic.h>
#else
#include <atomic>
#endif

//*************************** Forward Declarations **************************

//***************************************************************************

#ifdef __cplusplus
extern "C" {
#endif

//***************************************************************************

#define LUSH_DBG_STATS 0

// FIXME: used by GCC_ATTR_VAR_CACHE_ALIGN; move elsewhere
#define HOST_CACHE_LINE_SZ 64 /*L1*/

//***************************************************************************

#if (LUSH_DBG_STATS)
extern atomic_long DBG_numLockAcq;
extern atomic_long DBG_numLockAlloc;
extern atomic_long DBG_maxLockAllocCur;
extern atomic_long DBG_numLockFreelistCur;
#endif

typedef
#ifdef __cplusplus
  std::
#endif
  atomic_int atomic_pthread_spinlock_t;

typedef struct lushPtr_SyncObjData {

  union {
    atomic_pthread_spinlock_t spin; // usually an int
    //pthread_mutex_t  mutexlock
  } lock GCC_ATTR_VAR_CACHE_ALIGN;

#ifdef __cplusplus
  std::
#endif
  atomic_uint_least64_t idleness GCC_ATTR_VAR_CACHE_ALIGN;
  void*    cct_node;
  bool isBlockingWork;
  bool isLocked;

  struct lushPtr_SyncObjData* next;

} lushPtr_SyncObjData_t;


static inline void
lushPtr_SyncObjData_init(lushPtr_SyncObjData_t* x)
{
#ifdef __cplusplus
  using namespace std;
#endif
  atomic_store_explicit(&x->idleness, 0, memory_order_relaxed);
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
  int  cond_lock;  // 0 or the lock number on entry to cond-var critical region

  uint64_t doIdlenessCnt;
  uint64_t begIdleness; // begin idleness 'timestamp'
  uint64_t idleness;    // accumulated idleness

  // -------------------------------------------------------
  // process wide metrics
  // -------------------------------------------------------
#ifdef __cplusplus
  std::
#endif
  atomic_long* ps_num_procs;        // available processor cores
#ifdef __cplusplus
  std::
#endif
  atomic_long* ps_num_threads;

#ifdef __cplusplus
  std::
#endif
  atomic_long* ps_num_working;      // working (W_l) + (W_c) + (W_o)
#ifdef __cplusplus
  std::
#endif
  atomic_long* ps_num_working_lock; // working (W_l)
  //    ps_num_working_othr  // working (W_c) + (W_o)

  //    ps_num_idle_lock     // idleness (I_l)
#ifdef __cplusplus
  std::
#endif
  atomic_long* ps_num_idle_cond;    // idleness (I_cl) + (I_cv)

  // -------------------------------------------------------
  // LUSH_PTHR_FN_TY == 3
  // -------------------------------------------------------
  BalancedTree_t*        ps_syncObjToData; // synch-obj -> data
  BalancedTree_t         syncObjToData;    // synch-obj -> data

  lushPtr_SyncObjData_t* syncObjData;

  void*                  cache_syncObj;
  lushPtr_SyncObjData_t* cache_syncObjData;

  lushPtr_SyncObjData_t* freelstHead;
  lushPtr_SyncObjData_t* freelstTail;

} lushPthr_t;


//***************************************************************************

extern void* lushPthr_mem_beg; // memory begin
extern void* lushPthr_mem_end; // memory end
typedef
#ifdef __cplusplus
  std::
#endif
  atomic_uintptr_t lushPthr_mem_ptr_t;
extern lushPthr_mem_ptr_t lushPthr_mem_ptr; // current pointer


static inline void*
lushPthr_malloc(size_t size)
{
#ifdef __cplusplus
  using namespace std;
#endif
  char *memEnd = (char*)atomic_fetch_add_explicit(&lushPthr_mem_ptr, size, memory_order_relaxed);
  if (memEnd < (char *)lushPthr_mem_end) {
    return (memEnd - size);
  }
  return NULL;
}


// **************************************************************************

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif /* lush_pthreads_i */
