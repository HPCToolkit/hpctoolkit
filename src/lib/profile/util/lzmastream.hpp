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

#ifndef HPCTOOLKIT_PROFILE_UTIL_LZMASTREAM_H
#define HPCTOOLKIT_PROFILE_UTIL_LZMASTREAM_H

#include <lzma.h>

#include <istream>
#include <streambuf>
#include <type_traits>

namespace hpctoolkit::util {

/// Normal ordinary streambuf to work with a single LZMA/XZ stream
///
/// Currently only input (decompression) is supported
class lzmastreambuf : public std::streambuf {
public:
  explicit lzmastreambuf(std::streambuf*);
  ~lzmastreambuf();
  int_type underflow() override;

private:
  std::streambuf* base;
  lzma_stream stream;
  char* in_buffer;
  char* in_dz_buffer;
  bool tail;
};

namespace detail {
class lzmastream_base {
protected:
  lzmastreambuf zbuf;
  lzmastream_base(std::streambuf* buf) : zbuf(buf) {};
};
}

/// Simple std::istream that passes bits through lzmastreambuf
class ilzmastream : virtual detail::lzmastream_base, public std::istream {
public:
  explicit ilzmastream(std::streambuf* buf)
    : detail::lzmastream_base(buf), std::ios(&zbuf), std::istream(&zbuf) {};
  ~ilzmastream() = default;
};

}

#endif  // HPCTOOLKIT_PROFILE_UTIL_LZMASTREAM_H
