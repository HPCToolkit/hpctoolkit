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

#ifndef prof_lean_MCSLock_h
#define prof_lean_MCSLock_h

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
// MCSLockOp_t
// ---------------------------------------------------------

typedef enum MCSLockOp {

  MCSLockOp_NULL,
  MCSLockOp_read,
  MCSLockOp_write

} MCSLockOp_t;


static inline bool
MCSLockOp_isParallel(MCSLockOp_t x, MCSLockOp_t y)
{
  return (x == MCSLockOp_read && y == MCSLockOp_read);
};


// ---------------------------------------------------------
// MCSLockStatus_t
// ---------------------------------------------------------

typedef enum MCSLockStatus {

  MCSLockStatus_NULL,
  MCSLockStatus_Blocked,
  MCSLockStatus_SelfSignaled,
  MCSLockStatus_Signaled

} MCSLockStatus_t;


// ---------------------------------------------------------
// MCSLockLcl_t
// ---------------------------------------------------------

typedef struct MCSLockLcl {

  // A queue node
  struct MCSLockLcl* next;
  MCSLockStatus_t status;
  MCSLockOp_t op;

} MCSLockLcl_t;


static inline void 
MCSLockLcl_init(MCSLockLcl_t* x)
{
  x->next = NULL;
  x->status = MCSLockStatus_NULL;
  x->op = MCSLockOp_NULL;
}


//***************************************************************************
// MCSLock
//***************************************************************************

typedef struct MCSLock {

  MCSLockLcl_t* lock; // points to tail

} MCSLock_t;


static inline void 
MCSLock_init(MCSLock_t* x) 
{
  x->lock = NULL;
}


void
MCSLock_lock(MCSLock_t* lock, MCSLockLcl_t* lcl, MCSLockOp_t op);


void
MCSLock_unlock(MCSLock_t* lock, MCSLockLcl_t* lcl);


//***************************************************************************

#endif /* prof_lean_MCSLock_h */
