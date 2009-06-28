// -*-Mode: C++;-*- // technically C99
// $Id$

#ifndef _simple_lock_h_
#define _simple_lock_h_

#include <stdbool.h>

#include "cmpxchg.h"

/*
 * Simple spin lock.
 *
 */

typedef volatile long spinlock_t;
#define SPINLOCK_UNLOCKED ((spinlock_t) 0L)

static inline void 
spinlock_lock(spinlock_t *l)
{
  /* test-and-test-and-set lock */
  for(;;) {
    while (*l);
    if (cmpxchg(l, 0, 1) == 0) return; 
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


#endif  
