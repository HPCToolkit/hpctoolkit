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
// Copyright ((c)) 2002-2017, Rice University
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
//   A queueing reader-writer lock
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
// Author:
//   Nathan Tallent, John Mellor-Crummey, Rice University.
//
//***************************************************************************

//************************* System Include Files ****************************

//*************************** User Include Files ****************************

#include "QueuingRWLock.h"

//*************************** Forward Declarations **************************

//***************************************************************************
// 
//***************************************************************************

// TODO:tallent: Technically, some of these volatiles shouldn't be
// necessary and any compiler warnings should be eliminatd with casts.
// However, 1) at worse they add some extra memory loads; and 2) back
// in June 09, at least one of them appeared to be necessary for GCC
// 4.1.2 to generate correct code.  This should be revisited --
// possibly there was another explanation.

void
QueuingRWLock_lock(QueuingRWLock_t* lock, QueuingRWLockLcl_t* lcl, 
		   QueuingRWLockOp_t op)
{
  atomic_store_explicit(&lcl->next, NULL, memory_order_relaxed);
  atomic_store_explicit(&lcl->status, QueuingRWLockStatus_Blocked, memory_order_relaxed);
  lcl->op = op;

  QueuingRWLockLcl_t *prev = atomic_exchange_explicit(&lock->lock, lcl, memory_order_relaxed);
  if (prev) {
    // INVARIANT: queue was non-empty
    if (atomic_load_explicit(&prev->status, memory_order_relaxed) != QueuingRWLockStatus_Blocked 
	&& QueuingRWLockOp_isParallel(prev->op, op)) {
      // self-initiate early parallel start
      atomic_store_explicit(&lcl->status, QueuingRWLockStatus_SelfSignaled, memory_order_relaxed);
    }
    atomic_store_explicit(&prev->next, lcl, memory_order_relaxed);
    while (atomic_load_explicit(&lcl->status, memory_order_relaxed) == QueuingRWLockStatus_Blocked) {;} // spin
  }
  else {
    atomic_store_explicit(&lcl->status, QueuingRWLockStatus_Signaled, memory_order_relaxed);
  }
  // INVARIANT: lcl->status == SelfSignaled || Signaled
}


void
QueuingRWLock_unlock(QueuingRWLock_t* lock, QueuingRWLockLcl_t* lcl)
{
  while (atomic_load_explicit(&lcl->status, memory_order_relaxed) != QueuingRWLockStatus_Signaled) {;} // spin
  if (!atomic_load_explicit(&lcl->next, memory_order_relaxed)) {
    // INVARIANT: no known successor
    if (atomic_exchange_explicit(&lock->lock, lcl, memory_order_relaxed) == lcl) {
      return; // CAS returns *old* value iff successful
    }
    // another node is linking itself to me
    while (!atomic_load_explicit(&lcl->next, memory_order_relaxed)) {;}
  }
  // INVARIANT: a successor exists
  
  QueuingRWLockLcl_t* next = atomic_load_explicit(&lcl->next, memory_order_relaxed);
  QueuingRWLockStatus_t old_nextStatus = // begin passing baton
    atomic_exchange_explicit(&next->status, QueuingRWLockStatus_SelfSignaled, memory_order_relaxed);

  if (old_nextStatus == QueuingRWLockStatus_Blocked) {
    // initiate parallel start for successors of 'next'
    for (QueuingRWLockLcl_t* x = atomic_load_explicit(&next->next, memory_order_relaxed);
	 x && QueuingRWLockOp_isParallel(next->op, x->op); x = atomic_load_explicit(&x->next, memory_order_relaxed)) {
      atomic_store_explicit(&x->status, QueuingRWLockStatus_SelfSignaled, memory_order_relaxed);
    }
  }

  atomic_store_explicit(&next->status, QueuingRWLockStatus_Signaled, memory_order_relaxed); // complete passing baton
}
