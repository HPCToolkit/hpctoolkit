// -*-Mode: C++;-*- // technically C99
// $Id$

//***************************************************************************
//
// File: 
//   $Source$
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

#include "atomic-op.h"

//*************************** Forward Declarations **************************

//***************************************************************************
// 
//***************************************************************************

void
QueuingRWLock_lock(QueuingRWLock_t* lock, QueuingRWLockLcl_t* lcl, 
		   QueuingRWLockOp_t op)
{
  lcl->next = NULL;
  lcl->status = QueuingRWLockStatus_Blocked;
  lcl->op = op;

  QueuingRWLockLcl_t* prev = fetch_and_store_ptr(&lock->lock, lcl);
  if (prev) {
    // INVARIANT: queue was non-empty
    if (prev->status != QueuingRWLockStatus_Blocked 
	&& QueuingRWLockOp_isParallel(prev->op, op)) {
      // self-initiate early parallel start
      lcl->status = QueuingRWLockStatus_SelfSignaled;
    }
    prev->next = lcl;
    while (lcl->status == QueuingRWLockStatus_Blocked) {;} // spin
  }
  else {
    lcl->status = QueuingRWLockStatus_Signaled;
  }
  // INVARIANT: lcl->status == SelfSignaled || Signaled
}


void
QueuingRWLock_unlock(QueuingRWLock_t* lock, QueuingRWLockLcl_t* lcl)
{
  while (lcl->status != QueuingRWLockStatus_Signaled) {;} // spin
  if (!lcl->next) {
    // INVARIANT: no known successor
    if (compare_and_swap_ptr(&lock->lock, lcl, NULL) == lcl) {
      return; // CAS returns *old* value iff successful
    }
    // another node is linking itself to me
    while (!lcl->next) {;}
  }
  // INVARIANT: a successor exists
  
  volatile QueuingRWLockLcl_t* next = lcl->next;
  volatile QueuingRWLockStatus_t old_nextStatus = next->status;
  next->status = QueuingRWLockStatus_SelfSignaled; // begin passing baton

  if (old_nextStatus == QueuingRWLockStatus_Blocked) {
    // initiate parallel start for successors of 'next'
    for (volatile QueuingRWLockLcl_t* x = next->next;
	 x && QueuingRWLockOp_isParallel(next->op, x->op); x = x->next) {
      x->status = QueuingRWLockStatus_SelfSignaled;
    }
  }

  next->status = QueuingRWLockStatus_Signaled; // complete passing baton
}
