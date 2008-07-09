#ifndef _simple_lock_h_
#define _simple_lock_h_

/*
 * Simple spin lock.
 *
 */

typedef volatile long spinlock_t;

void spinlock_lock(spinlock_t *l);
void spinlock_unlock(spinlock_t *l);
int spinlock_is_locked(spinlock_t *l);

#define SPINLOCK_UNLOCKED ((spinlock_t) 0L)

#endif  
