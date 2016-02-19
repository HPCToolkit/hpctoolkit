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
//   Atomics.
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
// Author:
//   [...]
//
//***************************************************************************

#ifndef prof_lean_atomic_h
#define prof_lean_atomic_h

//************************* System Include Files ****************************

//*************************** User Include Files ****************************

#include "atomic-op.h"

#include <include/gcc-attr.h>

//*************************** Forward Declarations **************************

//***************************************************************************
// 
//***************************************************************************

// FIXME: tallent: RENAME

static inline long
csprof_atomic_increment(volatile long* addr)
{
  long old = fetch_and_add(addr, 1);
  return old;
}


static inline long
csprof_atomic_decrement(volatile long* addr)
{
  long old = fetch_and_add(addr, -1);
  return old;
}


static inline long
csprof_atomic_swap_l(volatile long* addr, long new)
{
  long old = fetch_and_store(addr, new);
  return old;
}


#define csprof_atomic_swap_p(vaddr, ptr) ((void*)csprof_atomic_swap_l((volatile long *)vaddr, (long) ptr))


// FIXME:tallent: this should be replace the "csprof" routines
// Careful: return the *new* value
#if (HPC_GCC_VERSION >= 4100)
#  define hpcrun_atomicIncr(x)      (void) __sync_add_and_fetch(x, 1)
#  define hpcrun_atomicDecr(x)      (void) __sync_sub_and_fetch(x, 1)
#  define hpcrun_atomicAdd(x, val)  __sync_add_and_fetch(x, val)
#  define hpcrun___sync_add_and_fetch

#else
#  warning "atomic.h: using slow atomics!"
#  define hpcrun_atomicIncr(x)      csprof_atomic_increment(x)
#  define hpcrun_atomicDecr(x)      csprof_atomic_decrement(x)
#  define hpcrun_atomicAdd(x, val)  fetch_and_add(x, val)
#endif


#endif // prof_lean_atomic_h
