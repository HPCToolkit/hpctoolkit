// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2018, Rice University
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

/***
 *** The code in this file is mainly taken from 
 *** https://github.com/deater/perf_event_tests/blob/master/include/perf_barrier.h
 ***/

#include <stdio.h>
#ifdef __powerpc__
#define rmb() asm volatile ("sync" : : : "memory")

#elif defined (__s390__)
#define rmb() asm volatile("bcr 15,0" ::: "memory")

#elif defined (__sh__)
#if defined(__SH4A__) || defined(__SH5__)
#define rmb()          asm volatile("synco" ::: "memory")
#else
#define rmb()          asm volatile("" ::: "memory")
#endif

#elif defined (__hppa__)
#define rmb()           asm volatile("" ::: "memory")

#elif defined (__sparc__)
#define rmb()           asm volatile("":::"memory")

#elif defined (__alpha__)
#define rmb()           asm volatile("mb" ::: "memory")

#elif defined(__ia64__)
#define rmb()           asm volatile ("mf" ::: "memory")

#elif defined(__arm__)
/*
 * Use the __kuser_memory_barrier helper in the CPU helper page. See
 * arch/arm/kernel/entry-armv.S in the kernel source for details.
 */
#define rmb()           ((void(*)(void))0xffff0fa0)()

//#if __LINUX_ARM_ARCH__ >= 7
//#define rmb() asm volatile("dsb " "" : : : "memory")
//#else
//#define rmb() asm volatile("mcr p15, 0, %0, c7, c10, 4" : : "r" (0) : "memory")
//#endif

#elif defined(__aarch64__)
#define rmb()           asm volatile("dmb ld" ::: "memory")
//#define rmb() asm volatile("dsb " "ld" : : : "memory")

#elif defined(__mips__)
#define rmb()           asm volatile(                                   \
                                ".set   mips2\n\t"                      \
                                "sync\n\t"                              \
                                ".set   mips0"                          \
				: /* no output */                       \
				: /* no input */                        \
				: "memory")

#elif defined(__i386__)
#define rmb() asm volatile("lock; addl $0,0(%%esp)" ::: "memory")

#elif defined(__x86_64)

#if defined(__KNC__)
#define rmb() __sync_synchronize()

#else
#define rmb() asm volatile("lfence":::"memory")
#endif

#else
#error Need to define rmb for this architecture!
#error See the kernel source directory: tools/perf/perf.h file
#endif

