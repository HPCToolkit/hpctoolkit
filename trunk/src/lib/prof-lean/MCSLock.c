// -*-Mode: C++;-*- // technically C99
// $Id$

//***************************************************************************
//
// File: 
//   $Source$
//
// Purpose:
//   A 'reader-writer' MCS lock
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

#include "MCSLock.h"

//*************************** Forward Declarations **************************

//***************************************************************************
// 
//***************************************************************************

void
MCSLock_lock(MCSLock_t* lock, MCSLockLcl_t* lcl, MCSLockOp_t op)
{
  lcl->next = NULL;
  lcl->status = MCSLockStatus_Blocked;
  lcl->op = op;

  MCSLockLcl_t* prev = fetch_and_store(&lock->lock, lcl);
  if (prev) {
    // queue was non-empty
    if (prev->status != MCSLockStatus_Blocked 
	&& MCSLockOp_isParallel(prev->op, op)) {
      lcl->status = MCSLockStatus_SelfSignaled;  // early parallel start
    }
    prev->next = lcl;
    while (lcl->status == MCSLockStatus_Blocked) ; // spin
  }
  else {
    lcl->status = MCSLockStatus_Signaled;
  }
  // INVARIANT: lcl->status == SelfSignaled || Signaled
}


void
MCSLock_unlock(MCSLock_t* lock, MCSLockLcl_t* lcl)
{
  while (lcl->status != MCSLockStatus_Signaled) ; // spin
  if (!lcl->next) {
    // no known successor
    if (compare_and_store(&lock->lock, lcl, NULL)) {
      return; // compare_and_store() returns true iff it stored
    }
    // another node is linking itself to me
    while (!lcl->next) ; // 
  }
  lcl->next->status = MCSLockStatus_Signaled;
}
