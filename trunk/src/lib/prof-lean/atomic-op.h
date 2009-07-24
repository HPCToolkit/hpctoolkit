// -*-Mode: C++;-*- // technically C99
// $Id$

//***************************************************************************
//
// File: atomic-op.h 
//
// Description:
//   define the small set of atomic operations needed by hpctoolkit
//
// Author:
//   23 July 2009 - John Mellor-Crummey
//     default to gnu atomics if they are available 
//
//     relegate the rest of the asm-based implementations to a separate
//     file
//
//***************************************************************************

#ifndef prof_lean_atomic_op_h
#define prof_lean_atomic_op_h

// if using gcc version >= 4.1, use gnu atomics 
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 1 )

#include "atomic-op-gcc.h"

#else

#include "atomic-op-asm.h"

#endif

#endif // prof_lean_atomic_op_h
