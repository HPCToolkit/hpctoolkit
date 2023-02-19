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

#include "lzmastream.hpp"

#include "log.hpp"

#include <cassert>
#include <cstring>

using namespace hpctoolkit::util;

#define BUFSIZE 4096

static lzma_ret maybeThrowLZMA_decoder(lzma_ret ret) {
  switch(ret) {
  case LZMA_OK:
  case LZMA_STREAM_END:
  // These aren't problems per-se
  case LZMA_NO_CHECK:
  case LZMA_UNSUPPORTED_CHECK:
  case LZMA_GET_CHECK:
    break;
  case LZMA_MEM_ERROR:
    log::debug{true} << "MEM_ERROR";
    throw std::runtime_error("LZMA decoder ran out of memory");
  case LZMA_FORMAT_ERROR:
  case LZMA_MEMLIMIT_ERROR:
    log::debug{true} << "FORMAT/MEMLIMIT_ERROR";
    throw std::runtime_error("LZMA decoder hit memory limit (the impossible happened?)");
  case LZMA_OPTIONS_ERROR:
    log::debug{true} << "OPTIONS_ERROR";
    throw std::runtime_error("LZMA decoder with wrong options");
  case LZMA_DATA_ERROR:
    log::debug{true} << "DATA_ERROR";
    throw std::runtime_error("attempt to decode a corrupted LZMA/XZ stream");
  case LZMA_BUF_ERROR:
    log::debug{true} << "BUF_ERROR";
    throw std::runtime_error("LZMA decoder failed (multiple times) to make progress (4K is too small a buffer size?)");
  case LZMA_PROG_ERROR:
    log::debug{true} << "PROG_ERROR";
    throw std::runtime_error("LZMA decoder encountered a really bad error");
  }
  return ret;
}


lzmastreambuf::lzmastreambuf(std::streambuf* base)
  : base(base), stream(LZMA_STREAM_INIT), in_buffer(new char[BUFSIZE]),
    in_dz_buffer(new char[BUFSIZE]), tail(false) {
  maybeThrowLZMA_decoder(lzma_auto_decoder(&stream, UINT64_MAX, 0));
  setg(nullptr, nullptr, nullptr);
}

lzmastreambuf::~lzmastreambuf() {
  lzma_end(&stream);
  delete[] in_buffer;
  delete[] in_dz_buffer;
}

lzmastreambuf::int_type lzmastreambuf::underflow() {
  using base_traits_type = std::remove_reference<decltype(*base)>::type::traits_type;
  if(gptr() == egptr()) {
    // The next bytes will land at the beginning of the buffer
    stream.next_out = (uint8_t*)in_dz_buffer;
    stream.avail_out = BUFSIZE;

    if(!tail) {
      // Shift the leftovers down to the start of the buffer
      std::memmove(in_buffer, stream.next_in, stream.avail_in);
      stream.next_in = (const uint8_t*)in_buffer;

      // Pull in some bytes for us to use
      stream.avail_in += base->sgetn(in_buffer + stream.avail_in, BUFSIZE - stream.avail_in);
    }

    // Run the decompression
    if(maybeThrowLZMA_decoder(lzma_code(&stream, LZMA_RUN)) == LZMA_STREAM_END
       && !tail) {
      // Put back all the bytes we don't need anymore
      for(size_t i = 0; i < stream.avail_in; i++) {
        if(base_traits_type::eq_int_type(base_traits_type::eof(), base->sungetc())) {
          log::debug{true} << "Failed to ungetc";
          // Welp, this is a problem
          setg(nullptr, nullptr, nullptr);
          return traits_type::eof();
        }
      }
      tail = true;
    }

    // Update the pointers as needed
    setg(in_dz_buffer, in_dz_buffer, (char*)stream.next_out);
  }
  return gptr() == egptr() ? traits_type::eof() : traits_type::not_eof(*gptr());
}
