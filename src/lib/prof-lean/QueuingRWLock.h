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
// Copyright ((c)) 2002-2016, Rice University
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
}


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
