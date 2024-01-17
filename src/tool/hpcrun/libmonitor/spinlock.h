/*
 *  Libmonitor spinlock.
 *
 *  Copyright (c) 2007-2023, Rice University.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 *  * Neither the name of Rice University (RICE) nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 *  This software is provided by RICE and contributors "as is" and any
 *  express or implied warranties, including, but not limited to, the
 *  implied warranties of merchantability and fitness for a particular
 *  purpose are disclaimed. In no event shall RICE or contributors be
 *  liable for any direct, indirect, incidental, special, exemplary, or
 *  consequential damages (including, but not limited to, procurement of
 *  substitute goods or services; loss of use, data, or profits; or
 *  business interruption) however caused and on any theory of liability,
 *  whether in contract, strict liability, or tort (including negligence
 *  or otherwise) arising in any way out of the use of this software, even
 *  if advised of the possibility of such damage.
 *
 *  $Id$
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
