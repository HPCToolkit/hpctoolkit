// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2010, Rice University 
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
