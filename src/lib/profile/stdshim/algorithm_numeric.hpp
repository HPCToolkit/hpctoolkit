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
// Copyright ((c)) 2019-2022, Rice University
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

#ifndef HPCTOOLKIT_STDSHIM_ALGORITHMNUMERIC_H
#define HPCTOOLKIT_STDSHIM_ALGORITHMNUMERIC_H

// This file is one of multiple "stdshim" headers, which act as a seamless
// transition library across versions of the STL. Mostly all this does is
// backport features from C++17 into C++11, sometimes by using class inheritance
// tricks, and sometimes by importing implementations from Boost or ourselves.
// Also see Google's Abseil project.

// This is the shim for <algorithm> and <numeric>.

#include "version.hpp"

#include <algorithm>
#include <numeric>

namespace hpctoolkit::stdshim {

// Identical to GCC 9.x-11.x STL implementation
template<class InIt, class OutIt, class T, class BinOp>
constexpr OutIt exclusive_scan(InIt first, InIt last, OutIt result, T init, BinOp op) {
  while(first != last) {
    auto v = init;
    init = op(init, *first);
    ++first;
    *result++ = std::move(v);
  }
  return result;
}
template<class InIt, class OutIt, class T>
constexpr OutIt exclusive_scan(InIt first, InIt last, OutIt result, T init) {
  return exclusive_scan(first, last, result, std::move(init), std::plus<>{});
}

}  // hpctoolkit::stdshim

#endif  // HPCTOOLKIT_STDSHIM_ALGORITHMNUMERIC_H
