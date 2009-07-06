// -*-Mode: C++;-*- // technically C99
// $Id$

//***************************************************************************
//
// File: 
//   $Source$
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

#include "atomic-op.h"

/*
 * Simple spin lock.
 *
 */

typedef volatile long spinlock_t;
#define SPINLOCK_UNLOCKED ((spinlock_t) 0L)
#define SPINLOCK_LOCKED ((spinlock_t) 1L)

static inline void 
spinlock_lock(spinlock_t *l)
{
  /* test-and-test-and-set lock */
  for(;;) {
    while (*l != SPINLOCK_UNLOCKED); 

    if (fetch_and_store(l, SPINLOCK_LOCKED) == SPINLOCK_UNLOCKED) {
      return; 
    }
  }
}


static inline void 
spinlock_unlock(spinlock_t *l)
{
  *l = SPINLOCK_UNLOCKED;
}


static inline bool 
spinlock_is_locked(spinlock_t *l)
{
  return (*l != SPINLOCK_UNLOCKED);
}


#endif // prof_lean_spinlock_h
