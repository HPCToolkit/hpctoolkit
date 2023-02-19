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

#include "stable_hash.hpp"

#include "../stdshim/bit.hpp"
#include <cassert>

using namespace hpctoolkit::util;

stable_hash_state::stable_hash_state()
  : state{
    0xc9fe'e7bb'75d4'e6e7ULL,
    0xf303'2e73'2f1e'20ddULL,
    0x4b67'c0a9'e090'9f41ULL,
    0x31fb'c679'ebed'6b59ULL,
  } {};

std::uint64_t stable_hash_state::squeeze() noexcept {
  pad();
  return state[3];
}

stable_hash_state& stable_hash_state::operator<<(std::uint64_t v) noexcept {
  assert(words_collected < word_rate);
  state[words_collected++] ^= v;
  if(words_collected >= word_rate)
    permute();
  return *this;
}

stable_hash_state& stable_hash_state::operator<<(std::string_view v) noexcept {
  for(std::size_t i = 8, e = v.size(); i < e; i += 8) {
    std::uint64_t x = ((std::uint64_t)v[i-8] << 0)
                    | ((std::uint64_t)v[i-7] << 8)
                    | ((std::uint64_t)v[i-6] << 16)
                    | ((std::uint64_t)v[i-5] << 24)
                    | ((std::uint64_t)v[i-4] << 32)
                    | ((std::uint64_t)v[i-3] << 40)
                    | ((std::uint64_t)v[i-2] << 48)
                    | ((std::uint64_t)v[i-1] << 56);
    *this << x;
  }
  std::size_t i = (v.size() / 8) * 8;
  std::size_t r = v.size() % 8;
  std::uint64_t x = ((std::uint64_t)(r <= 0 ? 0xb9 : v[i+0]) << 0)
                  | ((std::uint64_t)(r <= 1 ? 0x60 : v[i+1]) << 8)
                  | ((std::uint64_t)(r <= 2 ? 0x6b : v[i+2]) << 16)
                  | ((std::uint64_t)(r <= 3 ? 0x56 : v[i+3]) << 24)
                  | ((std::uint64_t)(r <= 4 ? 0x88 : v[i+4]) << 32)
                  | ((std::uint64_t)(r <= 5 ? 0x82 : v[i+5]) << 40)
                  | ((std::uint64_t)(r <= 6 ? 0xec : v[i+6]) << 48)
                  | ((std::uint64_t)0xf700'0000'0000'0000ULL);
  return *this << x;
}

void stable_hash_state::pad() noexcept {
  for(unsigned int i = words_collected; i < word_rate; ++i)
    *this << (std::uint64_t)0x153a'15f5'fc43'f283ULL;
  assert(words_collected == 0);
}

static const unsigned int s[64] = {
  7, 12, 17, 22, 39, 44, 49, 54, 7, 12, 17, 22, 39, 44, 49, 54,
  5, 9, 14, 20, 37, 41, 46, 52, 5, 9, 14, 20, 37, 41, 46, 52,
  4, 11, 16, 23, 36, 43, 48, 55, 4, 11, 16, 23, 36, 43, 48, 55,
  6, 10, 15, 21, 38, 42, 47, 53, 6, 10, 15, 21, 38, 42, 47, 53,
};
static const std::uint64_t k[64] = {
  0xd76aa4788a51407d, 0xe8c7b7566a88995d, 0x242070dbfd7025f4, 0xc1bdceeea7553036,
  0xf57c0faf489e15c1, 0x4787c62af5cdb84b, 0xa8304613c0ffbcf6, 0xfd469501253f7d7e,
  0x698098d8e93fd535, 0x8b44f7afd6cd6448, 0xffff5bb101220ae4, 0x895cd7bed806d023,
  0x6b901122e84e6ea9, 0xfd987193230135d8, 0xa679438ec27ae834, 0x49b40821f5292bf4,
  0xf61e256246711ac1, 0xc040b340a90a840a, 0x265e5a51fd1bbef0, 0xe9b6c7aa687810e5,
  0xd62f105d8c37fc1b, 0x02441453fffd6ec6, 0xd8a1e6818867beac, 0xe7d3fbc86c96fed4,
  0x21e1cde6fdbf77ac, 0xc33707d6a59c8134, 0xf4d50d874ac99be5, 0x455a14edf66d568a,
  0xa9e3e905bf80b2c1, 0xfcefa3f8277d05e4, 0x676f02d9ea2c8e1f, 0x8d2a4c8ad58fa982,
  0xfffa394203661adb, 0x8771f681d93be6ca, 0x6d9d6122e7585f52, 0xfde5380c20c23a76,
  0xa4beea44c3f22ce1, 0x4bdecfa9f47fb4d2, 0xf6bb4b604442b612, 0xbebfbc70aabc73eb,
  0x289b7ec6fcc24453, 0xeaa127fa66657006, 0xd4ef30858e1be7c3, 0x04881d05fff5bb27,
  0xd9d4d039867b8079, 0xe6db99e56ea336bb, 0x1fa27cf8fe09b282, 0xc4ac5665a3e07fda,
  0xf42922444cf3a208, 0x432aff97f708037e, 0xab9423a7bdfdd143, 0xfc93a03929b9c389,
  0x655b59c3eb1494a7, 0x8f0ccc92d44da632, 0xffeff47d05aa195e, 0x85845dd1da6ca20a,
  0x6fa87e4fe65dac20, 0xfe2ce6e01e8296e0, 0xa3014314c5658374, 0x4e0811a1f3d1564b,
  0xf7537e824212f2e5, 0xbd3af235ac6af723, 0x2ad7d2bbfc63b7e7, 0xeb86d3916450c165,
};

void stable_hash_state::permute() noexcept {
  assert(words_collected == word_rate);
  words_collected = 0;

  // The algorithm used here is like MD5 processing one 512-bit block, but:
  //  - The 4 words are 64-bit (instead of 32-bit),
  //  - The input is always 0,
  //  - Half of the shifts are adjusted by +32,
  //  - The sine-table is extended with 32-bit cosines, and
  //  - The resulting hash-state is not XOR'd with the previous hash-state.
  std::uint64_t& a = state[0];
  std::uint64_t& b = state[1];
  std::uint64_t& c = state[2];
  std::uint64_t& d = state[3];
  for(int i = 0; i < 64; ++i) {
    std::uint64_t f;
    if(i < 16) {
      f = (b & c) | ((~b) & d);
    } else if(i < 32) {
      f = (d & b) | ((~d) & c);
    } else if(i < 48) {
      f = b ^ c ^ d;
    } else {
      f = c ^ (b | (~d));
    }
    f ^= a ^ k[i];
    a = d;
    d = c;
    c = b;
    b ^= stdshim::rotl(f, s[i]);
  }
}
