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
// Copyright ((c)) 2002-2014, Rice University
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

#include "atomic-op.h"

/*
 * Simple spin lock.
 */

typedef struct spinlock_s {
	volatile long thelock;
} spinlock_t;

#define SPINLOCK_UNLOCKED_VALUE (-1L)
#define SPINLOCK_LOCKED_VALUE (1L)
#define INITIALIZE_SPINLOCK(x) { .thelock = (x) }

#define SPINLOCK_UNLOCKED INITIALIZE_SPINLOCK(SPINLOCK_UNLOCKED_VALUE)
#define SPINLOCK_LOCKED INITIALIZE_SPINLOCK(SPINLOCK_LOCKED_VALUE)


/*
 * Note: powerpc needs two memory barriers: isync at the end of _lock
 * and lwsync at the beginning of _unlock.  See JohnMC's Comp 422
 * slides on "IBM Power Weak Memory Model."
 *
 * Technically, the isync could be moved to the assembly code for
 * fetch_and_store().
 */
static inline void 
spinlock_lock(spinlock_t *l)
{
  /* test-and-test-and-set lock */
  for(;;) {
    while (l->thelock != SPINLOCK_UNLOCKED_VALUE); 

    if (fetch_and_store(&l->thelock, SPINLOCK_LOCKED_VALUE) == SPINLOCK_UNLOCKED_VALUE) {
      break;
    }
  }

#if defined(__powerpc__)
  __asm__ __volatile__ ("isync\n");
#endif
}


static inline void 
spinlock_unlock(spinlock_t *l)
{
#if defined(__powerpc__)
  __asm__ __volatile__ ("lwsync\n");
#endif

  l->thelock = SPINLOCK_UNLOCKED_VALUE;
}


static inline bool 
spinlock_is_locked(spinlock_t *l)
{
  return (l->thelock != SPINLOCK_UNLOCKED_VALUE);
}

// deadlock avoiding lock primitives:
//
// test-and-test-and-set lock acquisition, but make a bounded 
// number of attempts to get lock.
// This lock also permits multiple "locked" values
// returns false if lock not acquired
//
// NOTE: this lock is still released with spinlock_unlock operation
//
static inline bool
limited_spinlock_lock(spinlock_t* l, size_t limit, long locked_val)
{
  for(size_t i=0;;i++) {
    if (limit && (i > limit))
      return false;
    while (l->thelock != SPINLOCK_UNLOCKED_VALUE) {
      if (limit && (i > limit))
	return false;
      i++;
    }
    if (fetch_and_store(&l->thelock, locked_val) == SPINLOCK_UNLOCKED_VALUE)
      break;
  }

#if defined(__powerpc__)
  __asm__ __volatile__ ("isync\n");
#endif
  return true;
}

//
// test-and-test-and-set lock acquisition, but check
// the locked value to see if this lock has already been locked
// by the same thread (this should be the hardware thread id)
//
// returns false if lock not acquired
//
// NOTE: this lock is still released with spinlock_unlock operation
//
static inline bool
val_spinlock_lock(spinlock_t* l, size_t limit, long locked_val)
{
  for(;;) {
    // if we are already locked by the same id, prevent deadlock by
    // abandoning the try to lock
    if (l->thelock == locked_val)
      return false;
    while (l->thelock != SPINLOCK_UNLOCKED_VALUE) {
      if (l->thelock == locked_val)
	return false;
    }
    if (fetch_and_store(&l->thelock, locked_val) == SPINLOCK_UNLOCKED_VALUE)
      break;
  }

#if defined(__powerpc__)
  __asm__ __volatile__ ("isync\n");
#endif
  return true;
}

#endif // prof_lean_spinlock_h
