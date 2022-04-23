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
// Copyright ((c)) 2002-2022, Rice University
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

#include <stdint.h>

//------------------------------------------------------------------------------
// 64-bit powerpc load-linked and store-conditional
//
//   ldarx r_dest, addr_offset, r_addr
//   stdcx r_src,  addr_offset, r_addr
//------------------------------------------------------------------------------

#include "include/hpctoolkit-config.h"

#if SIZEOF_VOIDP == 8
#define LL  "ldarx"
#define SC  "stdcx."
#define CMP "cmpd"
#define ADD "add"
#endif

#if SIZEOF_VOIDP == 4
#define LL  "lwarx"
#define SC  "stwcx."
#define CMP "cmpw"
#define ADD "add"
#endif

#if !defined(LL)
#error "no definition for Power LL and SC primitives"
#endif

static inline void* fetch_and_store(volatile void** ptr, void* val) {
  void* result;
  __asm__ __volatile__(LL "  %0,0,%1 \n\t" SC "  %2,0,%1 \n\t"
                          "bne     $-8     \n\t"
                       : "=&r"(result)
                       : "r"(ptr), "r"(val)
                       : "cr0", "memory");
  return result;
}

static inline uint32_t fetch_and_add_i32(volatile uint32_t* ptr, uint32_t val) {
  uint32_t result;
  __asm__ __volatile__("lwarx   %0,0,%1 \n\t"
                       "add     %2,%2,%0   \n\t"
                       "stwcx.  %2,0,%1 \n\t"
                       "bne     $-12    \n\t"
                       : "=&r"(result)
                       : "r"(ptr), "r"(val)
                       : "cr0", "memory");
  return result;
}

static inline uint64_t fetch_and_add_i64(volatile uint64_t* ptr, uint64_t val) {
  uint64_t result;
  __asm__ __volatile__("ldarx   %0,0,%1 \n\t"
                       "add     %2,%2,%0   \n\t"
                       "stdcx.  %2,0,%1 \n\t"
                       "bne     $-12    \n\t"
                       : "=&r"(result)
                       : "r"(ptr), "r"(val)
                       : "cr0", "memory");
  return result;
}

static inline void* compare_and_swap_val(volatile void** ptr, void* oldval, void* newval) {
  void* result;
  __asm__ __volatile__(LL "  %0,0,%1 \n\t" CMP "  %0,%2   \n\t"
                          "bne     $+12    \n\t" SC "  %3,0,%1 \n\t"
                          "bne     $-16    \n\t"
                       : "=&r"(result)
                       : "r"(ptr), "r"(oldval), "r"(newval)
                       : "cr0", "memory");
  return result;
}

static inline bool compare_and_swap_bool(volatile void** ptr, void* oldval, void* newval) {
  void* resval = compare_and_swap_val(ptr, oldval, newval);
  bool result = oldval == resval;
  return result;
}
