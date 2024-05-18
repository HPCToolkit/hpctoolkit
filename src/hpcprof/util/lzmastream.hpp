// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

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
