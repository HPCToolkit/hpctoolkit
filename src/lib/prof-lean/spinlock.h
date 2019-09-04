// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2019, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

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

#include "stdatomic.h"

//
// Simple spin lock.
//

typedef struct spinlock_s {
	atomic_long thelock;
} spinlock_t;

#define SPINLOCK_UNLOCKED_VALUE (-1L)
#define SPINLOCK_LOCKED_VALUE (1L)
#define INITIALIZE_SPINLOCK(x) { .thelock = ATOMIC_VAR_INIT(x) }

#define SPINLOCK_UNLOCKED INITIALIZE_SPINLOCK(SPINLOCK_UNLOCKED_VALUE)
#define SPINLOCK_LOCKED INITIALIZE_SPINLOCK(SPINLOCK_LOCKED_VALUE)


static inline void
spinlock_init(spinlock_t *l)
{
  atomic_init(&l->thelock, SPINLOCK_UNLOCKED_VALUE);
}

static inline
void 
spinlock_unlock(spinlock_t *l)
{
  atomic_store_explicit(&l->thelock, SPINLOCK_UNLOCKED_VALUE, memory_order_release);
}


static inline
bool 
spinlock_is_locked(spinlock_t *l)
{
  return (atomic_load_explicit(&l->thelock, memory_order_relaxed) != SPINLOCK_UNLOCKED_VALUE);
}

static inline
void 
spinlock_lock(spinlock_t *l)
{
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
