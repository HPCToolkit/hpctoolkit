// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

//***************************************************************************
//
// File:
//   $HeadURL$
//
// Purpose:
//   Define an API for the MCS lock: a fair queue-based lock.
//
// Reference:
//   John M. Mellor-Crummey and Michael L. Scott. 1991. Algorithms for scalable
//   synchronization on shared-memory multiprocessors. ACM Transactions on
//   Computing Systems 9, 1 (February 1991), 21-65.
//   http://doi.acm.org/10.1145/103727.103729
//***************************************************************************



#ifndef _mcs_lock_h_
#define _mcs_lock_h_

//******************************************************************************
// global includes
//******************************************************************************

#include <stdbool.h>

#ifndef __cplusplus
#include <stdatomic.h>
#else
#include <atomic>
#endif


//******************************************************************************
// types
//******************************************************************************

typedef struct mcs_node_s {
#ifndef __cplusplus
  _Atomic(struct mcs_node_s*) next;
#else
  std::atomic<struct mcs_node_s*> next;
#endif

#ifdef __cplusplus
  std::
#endif
  atomic_bool blocked;
} mcs_node_t;


typedef struct {
#ifndef __cplusplus
  _Atomic(mcs_node_t *) tail;
#else
  std::atomic<mcs_node_t *> tail;
#endif
} mcs_lock_t;



//******************************************************************************
// macros
//******************************************************************************

#define MCS_LOCK_INITIALIZER { .tail = ATOMIC_VAR_INIT(NULL) }

#define MCS_LOCK_SYNCHRONIZED(LOCK, CODE)   \
  do {                                      \
    mcs_node_t mcs_node;                    \
    mcs_lock(&(LOCK), &mcs_node);           \
    { CODE }                                \
    mcs_unlock(&(LOCK), &mcs_node);         \
  } while (0)


//******************************************************************************
// constants
//******************************************************************************

#define mcs_nil (struct mcs_node_s*) 0

//******************************************************************************
// interface functions
//******************************************************************************

static inline void
mcs_init(mcs_lock_t *l)
{
#ifdef __cplusplus
  using namespace std;
#endif
  atomic_init(&l->tail, mcs_nil);
}


void
mcs_lock(mcs_lock_t *l, mcs_node_t *me);


bool
mcs_trylock(mcs_lock_t *l, mcs_node_t *me);


void
mcs_unlock(mcs_lock_t *l, mcs_node_t *me);

#endif
