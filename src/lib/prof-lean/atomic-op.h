// -*-Mode: C++;-*- // technically C99
// $Id$

//***************************************************************************
//
// File: 
//   $Source$
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

#ifndef prof_lean_atomic_op_h
#define prof_lean_atomic_op_h

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
    if (prev != old || store_conditional(ptr, new)) break;                  \
  }
#endif


#if defined(CAS_BODY) 

static inline unsigned long
compare_and_swap(volatile void *ptr, unsigned long old, unsigned long new)
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
compare_and_swap_i32(volatile int32_t *ptr, int32_t old, int32_t new)
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

//***************************************************************************

#if defined(FAS_BODY)

static inline unsigned long
fetch_and_store(volatile long* ptr, unsigned long new)
{
  unsigned long prev;
  FAS_BODY;
  return prev;
}


static inline unsigned long
fetch_and_store_i32(volatile int32_t* ptr, int32_t new)
{
  int32_t prev;
  FAS_BODY;
  return prev;
}

#else

static inline long
fetch_and_store(volatile long* addr, long new)
{
  long result;
  read_modify_write(long, addr, new, result);
  return result;
}

#endif

#define fetch_and_store_ptr(vaddr, ptr) \
  ((void *) fetch_and_store((volatile long *)vaddr, (long) ptr))

//***************************************************************************

#endif // prof_lean_atomic_op_h
