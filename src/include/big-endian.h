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

// This file defines the following macros for converting between host
// and big endian format.
//
//   host_to_be_16(uint16_t) --> uint16_t
//   host_to_be_32(uint32_t) --> uint32_t
//   host_to_be_64(uint64_t) --> uint64_t
//
//   be_to_host_16(uint16_t) --> uint16_t
//   be_to_host_32(uint32_t) --> uint32_t
//   be_to_host_64(uint64_t) --> uint64_t
//
// We prefer to use <byteswap.h> which usually exists.  This file
// contains assembler code for the fastest way to swap bytes on a
// given architecture.  And of course, on a big endian machine, the
// conversion functions are just the identity function.
//
// Notes:
// 1. All of the macros expect an unsigned int of the given size.
// It's not allowed to do bit operations (&, >>) on floating point,
// so if you need to convert a float, convert it to uint32_t first.
// Probably the best way to do that is with a union.
//
// 2. We could use htonl() from <arpa/inet.h> or htobe32() from
// <endian.h>.  But htonl() has no 64-bit version and <endian.h> is
// non-standard.  The purpose of this file is to provide a standard
// interface that works in all cases and uses the fast assembler code
// where possible.

//***************************************************************************

#ifndef include_big_endian_h
#define include_big_endian_h

#include <stdint.h>
#include <include/hpctoolkit-config.h>

//**************************************************
// Internal helper functions
//**************************************************

#ifdef USE_SYSTEM_BYTESWAP

#include <byteswap.h>

// If <byteswap.h> exists, then it contains assembler code for the
// fastest way to swap bytes.  We expect this to be the normal case.

#define _raw_byte_swap_16(x)  bswap_16(x)
#define _raw_byte_swap_32(x)  bswap_32(x)
#define _raw_byte_swap_64(x)  bswap_64(x)

#else

// If not, then a formula of masks and shifts is the fastest way I
// (krentel) can find.  This is much faster than a loop of shifts (too
// serial).

#define _raw_byte_swap_16(x)  \
  ( (((x) & 0xff00) >> 8)     \
  | (((x) & 0x00ff) << 8) )

#define _raw_byte_swap_32(x)    \
  ( (((x) & 0xff000000) >> 24)  \
  | (((x) & 0x00ff0000) >> 8)   \
  | (((x) & 0x0000ff00) << 8)   \
  | (((x) & 0x000000ff) << 24) )

#define _raw_byte_swap_64(x)  \
  ( (((x) & 0xff00000000000000) >> 56)  \
  | (((x) & 0x00ff000000000000) >> 40)  \
  | (((x) & 0x0000ff0000000000) >> 24)  \
  | (((x) & 0x000000ff00000000) >> 8)   \
  | (((x) & 0x00000000ff000000) << 8)   \
  | (((x) & 0x0000000000ff0000) << 24)  \
  | (((x) & 0x000000000000ff00) << 40)  \
  | (((x) & 0x00000000000000ff) << 56) )

#endif

//**************************************************
// Interface functions
//**************************************************

#ifdef HOST_BIG_ENDIAN

// On a big endian machine, the conversion functions are the identity
// function.

#define host_to_be_16(x)  ((uint16_t) (x))
#define host_to_be_32(x)  ((uint32_t) (x))
#define host_to_be_64(x)  ((uint64_t) (x))

#else

// On a little endian machine, we actually swap the bytes.

#define host_to_be_16(x)  _raw_byte_swap_16((uint16_t) (x))
#define host_to_be_32(x)  _raw_byte_swap_32((uint32_t) (x))
#define host_to_be_64(x)  _raw_byte_swap_64((uint64_t) (x))

#endif

// And in all cases, the conversion from big endian back to host order
// is the same as from host to big endian.

#define be_to_host_16(x)  host_to_be_16(x)
#define be_to_host_32(x)  host_to_be_32(x)
#define be_to_host_64(x)  host_to_be_64(x)

#endif  // include_big_endian_h
