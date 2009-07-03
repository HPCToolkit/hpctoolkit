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

#ifndef prof_lean_QueuingRWLock_h
#define prof_lean_QueuingRWLock_h

//************************* System Include Files ****************************

#include <stddef.h>
#include <stdbool.h>

//*************************** User Include Files ****************************

#include <include/uint.h>

//*************************** Forward Declarations **************************

//***************************************************************************
// 
//***************************************************************************

// ---------------------------------------------------------
// QueuingRWLockOp_t
// ---------------------------------------------------------

typedef enum QueuingRWLockOp {

  QueuingRWLockOp_NULL,
  QueuingRWLockOp_read,
  QueuingRWLockOp_write

} QueuingRWLockOp_t;


static inline bool
QueuingRWLockOp_isParallel(QueuingRWLockOp_t x, QueuingRWLockOp_t y)
{
  return (x == QueuingRWLockOp_read && y == QueuingRWLockOp_read);
};


// ---------------------------------------------------------
// QueuingRWLockStatus_t
// ---------------------------------------------------------

typedef enum QueuingRWLockStatus {

  QueuingRWLockStatus_NULL,
  QueuingRWLockStatus_Blocked,
  QueuingRWLockStatus_SelfSignaled,
  QueuingRWLockStatus_Signaled

} QueuingRWLockStatus_t;


// ---------------------------------------------------------
// QueuingRWLockLcl_t
// ---------------------------------------------------------

typedef struct QueuingRWLockLcl {

  // A queue node
  volatile struct QueuingRWLockLcl* next;
  volatile QueuingRWLockStatus_t status;
  QueuingRWLockOp_t op;

} QueuingRWLockLcl_t;


static inline void 
QueuingRWLockLcl_init(QueuingRWLockLcl_t* x)
{
  x->next = NULL;
  x->status = QueuingRWLockStatus_NULL;
  x->op = QueuingRWLockOp_NULL;
}


//***************************************************************************
// QueuingRWLock
//***************************************************************************

typedef struct QueuingRWLock {

  volatile QueuingRWLockLcl_t* lock; // points to tail

} QueuingRWLock_t;


static inline void 
QueuingRWLock_init(QueuingRWLock_t* x) 
{
  x->lock = NULL;
}


void
QueuingRWLock_lock(QueuingRWLock_t* lock, QueuingRWLockLcl_t* lcl, 
		   QueuingRWLockOp_t op);


void
QueuingRWLock_unlock(QueuingRWLock_t* lock, QueuingRWLockLcl_t* lcl);


//***************************************************************************

#endif /* prof_lean_QueuingRWLock_h */
