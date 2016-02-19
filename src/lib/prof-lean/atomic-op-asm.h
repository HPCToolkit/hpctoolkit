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

#ifndef prof_lean_atomic_op_asm_h
#define prof_lean_atomic_op_asm_h

//************************* System Include Files ****************************

#include <stdint.h>
#include <assert.h>

//*************************** User Include Files ****************************

#include "atomic-op.i"

//***************************************************************************

#if defined(LL_BODY)

inline static unsigned long 
load_linked(volatile unsigned long* ptr)
{
  volatile unsigned long result;
  LL_BODY;
  return result;
}

#endif

//***************************************************************************

#if defined(SC_BODY) 

/* if successful, return 1 */
inline static int 
store_conditional(volatile unsigned long* ptr, unsigned long val)
{
  int result;
  SC_BODY;
  return result;
}

#endif

//***************************************************************************

#if defined (LL_BODY) && defined(SC_BODY) && !defined(CAS_BODY)

#define CAS_BODY                                                            \
  for(;;) {                                                                 \
    prev = load_linked(ptr);                                                \
    if (prev != oldval || store_conditional(ptr, newval)) break;            \
  }
#endif


#if defined(CAS_BODY) 

static inline unsigned long
compare_and_swap(volatile void *ptr, unsigned long oldval, unsigned long newval)
{
  unsigned long prev;
  CAS_BODY;
  return prev;
}


#define compare_and_swap_ptr(vptr, oldptr, newptr)			\
  ((void*) compare_and_swap((volatile void*)vptr,			\
			    (unsigned long)oldptr, (unsigned long)newptr))

#endif


static inline int32_t
compare_and_swap_i32(volatile int32_t *ptr, int32_t oldval, int32_t newval)
{
  int32_t prev;
#if defined (CAS4_BODY)
  CAS4_BODY;
#else
# warning "compare_and_swap_i32() is unimplemented!"
  assert(0 && "compare_and_swap_i32() is unimplemented!");
#endif
  return prev;

}


//***************************************************************************

#if defined (LL_BODY) && defined(SC_BODY) 

#define read_modify_write(type, addr, expn, result) {			\
  type __new;								\
  do {								        \
    result = (type) load_linked((unsigned long*)addr);	         	\
    __new = expn;							\
  } while (!store_conditional((unsigned long*)addr, (unsigned long) __new)); \
}

#else

#define read_modify_write(type, addr, expn, result) {                        \
    type __new;                                                              \
    do {                                                                     \
      result = *addr;                                                        \
      __new = expn;                                                          \
    } while (compare_and_swap(addr, result, __new) != result);               \
}

#endif

//***************************************************************************

static inline long
fetch_and_add(volatile long* addr, long val)
{
  long result;
  read_modify_write(long, addr, result + val, result);
  return result;
}


// FIXME: hack to correspond to atomic-op-gcc
static inline void
atomic_add_i64(volatile long* addr, long val)
{
  fetch_and_add(addr, val);
}

//***************************************************************************

#if defined(FAS_BODY)

static inline unsigned long
fetch_and_store(volatile long* ptr, unsigned long newval)
{
  unsigned long prev;
  FAS_BODY;
  return prev;
}


static inline unsigned long
fetch_and_store_i32(volatile int32_t* ptr, int32_t newval)
{
  int32_t prev;
  FAS_BODY;
  return prev;
}

// FIXME: hack to correspond to atomic-op-gcc
static inline unsigned long
fetch_and_store_i64(volatile int64_t* ptr, int64_t newval)
{
  return fetch_and_store(ptr, newval);
}

#else

static inline long
fetch_and_store(volatile long* addr, long newval)
{
  long result;
  read_modify_write(long, addr, newval, result);
  return result;
}

#endif

#define fetch_and_store_ptr(vaddr, ptr) \
  ((void *) fetch_and_store((volatile long *)vaddr, (long) ptr))


//***************************************************************************

#endif // prof_lean_atomic_op_asm_h
