/*
 * Simple spin lock.
 *
 * $Id$
 */

#include "spinlock.h"
#include "cmpxchg.h"

void
spinlock_lock(spinlock_t *l)
{
  /* test-and-test-and-set lock */
  for(;;) {
    while (*l);
    if (cmpxchg(l, 0, 1) == 0) return; 
  }
}


void
spinlock_unlock(spinlock_t *l)
{
    *l = 0;
}


int 
spinlock_is_locked(spinlock_t *l)
{
   return (int) *l;
}
