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

#include <include/gcc-attr.h>


#if (GCC_VERSION >= 4100)

#  include "atomic-op-gcc.h"

#else

#  include "atomic-op-asm.h"

#endif

#endif // prof_lean_atomic_op_h
