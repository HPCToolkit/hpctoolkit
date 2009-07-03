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
    // queue was non-empty
    if (prev->status != QueuingRWLockStatus_Blocked 
	&& QueuingRWLockOp_isParallel(prev->op, op)) {
      lcl->status = QueuingRWLockStatus_SelfSignaled;  // early parallel start
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
    // no known successor
    if (compare_and_swap_ptr(&lock->lock, lcl, NULL) == lcl) {
      return; // CAS returns *old* value iff successful
    }
    // another node is linking itself to me
    while (!lcl->next) {;}
  }
  lcl->next->status = QueuingRWLockStatus_Signaled;
}
