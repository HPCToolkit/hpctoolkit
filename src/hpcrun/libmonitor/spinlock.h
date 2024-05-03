// SPDX-FileCopyrightText: 2007-2023 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

/*
 *  Libmonitor spinlock.
 */

#ifndef  _MONITOR_SPINLOCK_
#define  _MONITOR_SPINLOCK_

#include "atomic.h"

#define SPINLOCK_UNLOCKED  { .val = 0L }
#define SPINLOCK_LOCKED    { .val = 1L }

struct monitor_spinlock {
    volatile long val;
};

typedef struct monitor_spinlock spinlock_t;

/*
 *  Test and test and set spinlock.
 *
 *  Note: powerpc needs two memory barriers: isync at the end of _lock
 *  and _trylock and lwsync at the beginning of _unlock.  See JohnMC's
 *  Comp 422 slides on "IBM Power Weak Memory Model."
 */
static inline void
spinlock_lock(spinlock_t *lock)
{
    for (;;) {
        while (lock->val != 0)
            ;
        if (compare_and_swap(&lock->val, 0, 1) == 0) {
            break;
        }
    }

#if defined(__powerpc__)
    __asm__ __volatile__ ("isync\n");
#endif
}

static inline void
spinlock_unlock(spinlock_t *lock)
{
#if defined(__powerpc__)
    __asm__ __volatile__ ("lwsync\n");
#endif

    lock->val = 0;
}

/*
 *  Returns 0 on success (acquire lock), else -1 on failure,
 *  same as pthread_mutex_trylock().
 */
static inline int
spinlock_trylock(spinlock_t *lock)
{
    if (compare_and_swap(&lock->val, 0, 1) == 0) {
#if defined(__powerpc__)
        __asm__ __volatile__ ("isync\n");
#endif
        return 0;
    }

    return -1;
}

#endif  /* _MONITOR_SPINLOCK_ */
