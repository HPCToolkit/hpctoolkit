/*
 *  Libmonitor atomic ops.
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

#ifndef  _MONITOR_ATOMIC_OPS_
#define  _MONITOR_ATOMIC_OPS_

/*
 *  The API that libmonitor supports.
 */
static inline long
compare_and_swap(volatile long *ptr, long old, long new);


/*
 *  We prefer the GNU builtin atomic ops.  If not, then provide
 *  assembler code for some common platforms.
 */
#if defined(USE_GNU_ATOMIC_OPS)

static inline long
compare_and_swap(volatile long *ptr, long old, long new)
{
    return __sync_val_compare_and_swap(ptr, old, new);
}

#elif defined(__x86_64__)

static inline long
compare_and_swap(volatile long *ptr, long old, long new)
{
    long prev;

    __asm__ __volatile__ (
	"\tlock; cmpxchgq %3, (%1)"
	: "=a" (prev) : "r" (ptr), "a" (old), "r" (new) : "memory"
	);

    return prev;
}

#elif defined(__i386__)

static inline long
compare_and_swap(volatile long *ptr, long old, long new)
{
    long prev;

    __asm__ __volatile__ (
	"\tlock; cmpxchg %3, (%1)"
	: "=a" (prev) : "r" (ptr), "a" (old), "r" (new) : "memory"
	);

    return prev;
}

#elif defined(__powerpc__)

#if defined(__powerpc64__)

static inline long
load_linked(volatile long *ptr)
{
    volatile long ret;

    __asm__ __volatile__ (
	"ldarx %0,0,%1" : "=r" (ret) : "r" (ptr)
	);

    return ret;
}

static inline int
store_conditional(volatile long *ptr, long val)
{
    int ret;

    __asm__ __volatile__ (
	"stdcx. %2,0,%1 \n\t"
	"bne    $+12    \n\t"
	"li     %0,1    \n\t"
	"b      $+8     \n\t"
	"li     %0,0" : "=&r" (ret) : "r" (ptr), "r" (val) : "cr0", "memory"
	);

    return ret;
}

#else  /* 32-bit powerpc */

static inline long
load_linked(volatile long *ptr)
{
    volatile long ret;

    __asm__ __volatile__ (
	"lwarx %0,0,%1" : "=r" (ret) : "r" (ptr)
	);

    return ret;
}

static inline int
store_conditional(volatile long *ptr, long val)
{
    volatile int ret;

    __asm__ __volatile__ (
	"stwcx. %2,0,%1 \n\t"
	"bne    $+12    \n\t"
	"li     %0,1    \n\t"
	"b      $+8     \n\t"
	"li     %0,0" : "=&r" (ret) : "r" (ptr), "r" (val) : "cr0", "memory"
	);

    return ret;
}

#endif

/*
 *  Common to 64 and 32-bit powerpc.
 */
static inline long
compare_and_swap(volatile long *ptr, long old, long new)
{
    long prev;

    for (;;) {
	prev = load_linked(ptr);
	if (prev != old || store_conditional(ptr, new)) {
	    break;
	}
    }

    return prev;
}

#elif defined(__ia64__)

static inline long
compare_and_swap(volatile long *ptr, long old, long new)
{
    long prev;

    __asm__ __volatile__ (
	"mov ar.ccv=%0;;" :: "rO" (old)
	);
    __asm__ __volatile__ (
	"cmpxchg8.acq %0=[%1],%2,ar.ccv"
	: "=r" (prev) : "r" (ptr), "r" (new) : "memory"
	);

    return prev;
}

#else

#error unknown platform for atomic compare and swap -- \
use a later version of gcc that supports the GNU atomic ops

#endif

#endif  /* _MONITOR_ATOMIC_OPS_ */
