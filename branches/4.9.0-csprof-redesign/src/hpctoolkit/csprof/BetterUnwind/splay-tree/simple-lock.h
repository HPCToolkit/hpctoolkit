/*
 * Simple spin lock.
 *
 * $Id$
 */

#ifndef _SIMPLE_LOCK_H_
#define _SIMPLE_LOCK_H_

typedef long simple_spinlock;

void simple_spinlock_lock(simple_spinlock *l);
void simple_spinlock_unlock(simple_spinlock *l);

#endif  /* !_SIMPLE_LOCK_H_ */
