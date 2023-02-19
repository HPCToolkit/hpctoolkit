// -*-Mode: C++;-*-

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
// Copyright ((c)) 2002-2023, Rice University
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

#ifndef HPCTOOLKIT_STDSHIM_BIT_H
#define HPCTOOLKIT_STDSHIM_BIT_H

// This file is one of multiple "stdshim" headers, which act as a seamless
// transition library across versions of the STL. Mostly all this does is
// backport features from C++17 into C++11, sometimes by using class inheritance
// tricks, and sometimes by importing implementations from Boost or ourselves.
// Also see Google's Abseil project.

// This is the shim for <bit>.

#include "version.hpp"

#if defined(HPCTOOLKIT_STDSHIM_STD_HAS_bit)
#include <bit>

namespace hpctoolkit::stdshim {
// TODO: Provide full implementations of commented functions as well
// using std::endian;
// using std::bit_cast;
// using std::byteswap;
// using std::has_single_bit;
// using std::bit_ceil;
// using std::bit_floor;
// using std::bit_width;
using std::rotl;
using std::rotl;
// using std::countl_zero;
// using std::countl_one;
// using std::countr_zero;
// using std::countr_one;
// using std::popcount;
}

#else  // HPCTOOLKIT_STDSHIM_STD_HAS_bit

#include <limits>

namespace hpctoolkit::stdshim {

/// Circular-rotate left by the given number of bits
template<class T>
[[nodiscard]] constexpr
std::enable_if_t<std::is_integral_v<T> && !std::is_signed_v<T>, T>
rotl(T v, int b) {
  constexpr auto nbits = std::numeric_limits<T>::digits;
  if constexpr ((nbits & (nbits - 1)) == 0) {
    // Power of 2 rotation. Compilers tend to optimize this away
    constexpr unsigned int u_nbits = nbits;
    const unsigned int u_b = b;
    return (v << (u_b % u_nbits)) | (v >> ((-u_b) % u_nbits));
  }
  // Standards-compliant slow path
  const int m_b = b % nbits;
  if(m_b == 0) {
    return v;
  } else if (m_b > 0) {
    return (v << m_b) | (v >> ((nbits - m_b) % nbits));
  }
  return (v >> -m_b) | (v << ((nbits + m_b) % nbits));  // rotr(v, -b)
}

/// Circular-rotate right by the given number of bits
template<class T>
[[nodiscard]] constexpr
std::enable_if_t<std::is_integral_v<T> && !std::is_signed_v<T>, T>
rotr(T v, int b) {
  constexpr auto nbits = std::numeric_limits<T>::digits;
  if constexpr ((nbits & (nbits - 1)) == 0) {
    // Power of 2 rotation. Compilers tend to optimize this away
    constexpr unsigned int u_nbits = nbits;
    const unsigned int u_b = b;
    return (v >> (u_b % u_nbits)) | (v << ((-u_b) % u_nbits));
  }
  // Standards-compliant slow path
  const int m_b = b % nbits;
  if(m_b == 0) {
    return v;
  } else if (m_b > 0) {
    return (v >> m_b) | (v << ((nbits - m_b) % nbits));
  }
  return (v << -m_b) | (v >> ((nbits + m_b) % nbits));  // rotl(v, -b)
}

} // namespace hpctoolkit::stdshim

#endif  // HPCTOOLKIT_STDSHIM_STD_HAS_bit

#endif  // HPCTOOLKIT_STDSHIM_BIT_H
