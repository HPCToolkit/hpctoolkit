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

#ifndef _fence_h_
#define _fence_h_

#if defined(__powerpc__)
//------------------------------------------------------------------------------
// According to Power ISA version 2.07B, April 2015, pages 833-834
//
// B.2.1 Lock Acquisition and Import Barriers
// An “import barrier” is an instruction or sequence of instructions
// that prevents storage accesses caused by instructions following the
// barrier from being performed before storage accesses that acquire a
// lock have been performed. An import barrier can be used to ensure that
// a shared data structure protected by a lock is not accessed until the
// lock has been acquired.
//
// B.2.1.1 Acquire Lock and Import Shared Storage
// If lwarx and stwcx. instructions are used to obtain the lock, an import
// barrier can be constructed by placing an isync instruction immediately
// following the loop containing the lwarx and stwcx..
//
// B.2.2 Lock Release and Export Barriers
// An “export barrier” is an instruction or sequence of instructions that
// prevents the store that releases a lock from being performed before
// stores caused by instructions preceding the barrier have been performed.
// An export barrier can be used to ensure that all stores to a shared data
// structure protected by a lock will be performed with respect to any other
// processor before the store that releases the lock is performed with
// respect to that processor.
//
// B.2.2.2 Export Shared Storage and Release Lock using lwsync
// If the shared data structure is in storage that is neither Write Through
// Required nor Caching Inhibited, an lwsync instruction can be used as
// the export barrier.
//
// B.2.3 Safe Fetch
// If a load must be performed before a subsequent store (e.g., the store
// that releases a lock protecting a shared data structure), a technique
// similar to the following can be used
//
//   [example with cmpw and bne- omitted]
//
// If both storage operands are in storage that is neither Write Through
// Required nor Caching Inhibited, another alternative is to replace the
// cmpw and bne- with an lwsync instruction.

//------------------------------------------------------------------------------

// isync -> L, S (B.2.1.1)
#define enforce_rmw_to_access_order() __asm__ __volatile__("isync\n")

// isync -> L (B.2.1.1)
#define enforce_rmw_to_load_order() __asm__ __volatile__("isync\n")

// L -> lwsync (B.2.3); S -> lwsync (B.2.2.2)
#define enforce_access_to_rmw_order() __asm__ __volatile__("lwsync\n")

// L -> lwsync (B.2.3)
#define enforce_load_to_rmw_order() __asm__ __volatile__("lwsync\n")

// S -> lwsync (B.2.2.2)
#define enforce_store_to_rmw_order() __asm__ __volatile__("lwsync\n")

// isync -> L, S (B.2.1.1)
#define enforce_load_to_access_order() __asm__ __volatile__("isync\n")

// isync -> L (B.2.1.1)
#define enforce_load_to_load_order() __asm__ __volatile__("isync\n")

// see table
#define enforce_load_to_load_and_store_to_store_order() __asm__ __volatile__("lwsync\n")

// L -> lwsync (B.2.3); S -> lwsync (B.2.2.2)
#define enforce_access_to_store_order() __asm__ __volatile__("lwsync\n")

// S -> lwsync (B.2.2.2)
#define enforce_store_to_store_order() __asm__ __volatile__("lwsync\n")

#endif

#if defined(__x86_64__)
//------------------------------------------------------------------------------
// According to Intel® 64 and IA-32 Architectures Software Developer’s Manual
// Volume 3:  System Programming Guide, February 2014
//
// Section 8.2.3.2 Neither Loads Nor Stores Are Reordered with Like Operations
// L -> L; S -> S
//
// Section 8.2.3.3 Stores Are Not Reordered With Earlier Loads
// L -> S
//
// Section 8.2.3.9 Loads and Stores are Not Reordered with Locked Instructions
// RMW -> L, S;  L, S -> RMW
//------------------------------------------------------------------------------

// no-op: RMW -> L, S by 8.2.3.9
#define enforce_rmw_to_access_order()

// no-op: RMW -> L by 8.2.3.9
#define enforce_rmw_to_load_order()

// no-op: L, S -> RMW by 8.2.3.9
#define enforce_access_to_rmw_order()

// no-op: L -> RMW by 8.2.3.9
#define enforce_load_to_rmw_order()

// no-op: S -> RMW by 8.2.3.9
#define enforce_store_to_rmw_order()

// no-op: L -> L by 8.2.3.2; L -> S by 8.2.3.3
#define enforce_load_to_access_order()

// no-op: L -> L by 8.2.3.2
#define enforce_load_to_load_order()

// no-op: S -> S by 8.2.3.2; L -> S by 8.2.3.3
#define enforce_access_to_store_order()

// no-op: S -> S by 8.2.3.2
#define enforce_store_to_store_order()

// no-op: S -> S and L -> L by 8.2.3.2
#define enforce_load_to_load_and_store_to_store_order()

#endif

#ifndef enforce_access_to_store_order
#error missing fence definitions: unknown processor type
#endif

#endif
