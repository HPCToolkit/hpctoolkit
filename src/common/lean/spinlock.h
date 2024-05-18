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
//   Spin lock
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
// Author:
//   [...]
//
//***************************************************************************

#ifndef prof_lean_spinlock_h
#define prof_lean_spinlock_h

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifndef __cplusplus
#include <stdatomic.h>
#else
#include <atomic>
#endif

//
// Simple spin lock.
//

typedef struct spinlock_s {
#ifdef __cplusplus
  std::
#endif
  atomic_long thelock;
} spinlock_t;

#define SPINLOCK_UNLOCKED_VALUE (-1L)
#define SPINLOCK_LOCKED_VALUE (1L)
#define INITIALIZE_SPINLOCK(x) { .thelock = x }

#define SPINLOCK_UNLOCKED INITIALIZE_SPINLOCK(SPINLOCK_UNLOCKED_VALUE)
#define SPINLOCK_LOCKED INITIALIZE_SPINLOCK(SPINLOCK_LOCKED_VALUE)


static inline void
spinlock_init(spinlock_t *l)
{
#ifdef __cplusplus
  using namespace std;
#endif
  atomic_init(&l->thelock, SPINLOCK_UNLOCKED_VALUE);
}

static inline
void
spinlock_unlock(spinlock_t *l)
{
#ifdef __cplusplus
  using namespace std;
#endif
  atomic_store_explicit(&l->thelock, SPINLOCK_UNLOCKED_VALUE, memory_order_release);
}


static inline
bool
spinlock_is_locked(spinlock_t *l)
{
#ifdef __cplusplus
  using namespace std;
#endif
  return (atomic_load_explicit(&l->thelock, memory_order_relaxed) != SPINLOCK_UNLOCKED_VALUE);
}

static inline
void
spinlock_lock(spinlock_t *l)
{
#ifdef __cplusplus
  using namespace std;
#endif
  /* test-and-set lock */
  long old_lockval = SPINLOCK_UNLOCKED_VALUE;
  while (!atomic_compare_exchange_weak_explicit(&l->thelock, &old_lockval, SPINLOCK_LOCKED_VALUE,
                                                memory_order_acquire, memory_order_relaxed)) {
    old_lockval = SPINLOCK_UNLOCKED_VALUE;
  }
}


// deadlock avoiding lock primitives:
//
// test-and-test-and-set lock acquisition, but make a bounded
// number of attempts to get lock.
// to be signature compatible, this locking function takes a "locked" value, but does
// not use it.
// returns false if lock not acquired
//
// NOTE: this lock is still released with spinlock_unlock operation
//
static inline
bool
limit_spinlock_lock(spinlock_t* l, size_t limit, long locked_val)
{
#ifdef __cplusplus
  using namespace std;
#endif
  if (limit == 0) {
    spinlock_lock(l);
    return true;
  }

  long old_lockval = SPINLOCK_UNLOCKED_VALUE;
  while (limit-- > 0 &&
         !atomic_compare_exchange_weak_explicit(&l->thelock, &old_lockval, SPINLOCK_LOCKED_VALUE,
                                                memory_order_acquire, memory_order_relaxed)) {
    old_lockval = SPINLOCK_UNLOCKED_VALUE;
  }
  return (limit + 1 > 0);
}

//
// hwt_cas_spinlock_lock uses the CAS primitive, but prevents deadlock
// by checking the locked value to see if this (hardware) thread has already
// acquired the lock
//
// returns false if lock acquisition is abandoned
//
// NOTE: this lock is still released with spinlock_unlock operation
//
static inline
bool
hwt_cas_spinlock_lock(spinlock_t* l, size_t limit, long locked_val)
{
#ifdef __cplusplus
  using namespace std;
#endif
  long old_lockval = SPINLOCK_UNLOCKED_VALUE;
  while (!atomic_compare_exchange_weak_explicit(&l->thelock, &old_lockval, locked_val,
                                                memory_order_acquire, memory_order_relaxed)) {
    // if we are already locked by the same id, prevent deadlock by
    // abandoning lock acquisition
    if (old_lockval == locked_val)
      return false;
    old_lockval = SPINLOCK_UNLOCKED_VALUE;
  }
  return true;
}

//
// hwt_limit_spinlock_lock uses the CAS primitive, but prevents deadlock
// by checking the locked value to see if this (hardware) thread has already
// acquired the lock. This spinlock *also* uses the iteration limit
// from the limit spinlock.

// The hwt technique covers priority inversion deadlock
// in a way that is most friendly to high priority threads.
//
// The iteration limit technique covers other kinds of deadlock
// and starvation situations.
//
// NOTE: this lock is still released with spinlock_unlock operation
//
static inline
bool
hwt_limit_spinlock_lock(spinlock_t* l, size_t limit, long locked_val)
{
#ifdef __cplusplus
  using namespace std;
#endif
  if (limit == 0)
    return hwt_cas_spinlock_lock(l, limit, locked_val);

  long old_lockval = SPINLOCK_UNLOCKED_VALUE;
  while (limit-- > 0 &&
         !atomic_compare_exchange_weak_explicit(&l->thelock, &old_lockval, locked_val,
                                                memory_order_acquire, memory_order_relaxed)) {
    // if we are already locked by the same id, prevent deadlock by
    // abandoning lock acquisition
    if (old_lockval == locked_val)
      return false;
    old_lockval = SPINLOCK_UNLOCKED_VALUE;
  }
  return (limit + 1 > 0);
}
#endif // prof_lean_spinlock_h
