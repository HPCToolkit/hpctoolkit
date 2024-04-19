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
// Copyright ((c)) 2002-2024, Rice University
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

#ifndef HPCTOOLKIT_PROFILE_FORMATS_PRIMITIVES_H
#define HPCTOOLKIT_PROFILE_FORMATS_PRIMITIVES_H

#include "core.hpp"

#include "../../../lib/prof-lean/formats/primitive.h"

#include <cstdint>
#include <cassert>
#include <string>
#include <string_view>

namespace hpctoolkit::formats {

template<>
struct DataTraits<std::uint16_t> final : ConstSizeDataTraits<std::uint16_t, 2> {
  ser_type serialize(const std::uint16_t& data) noexcept {
    ser_type buf;
    fmt_u16_write(buf.data(), data);
    return buf;
  }
};

template<>
struct DataTraits<std::uint32_t> final : ConstSizeDataTraits<std::uint32_t, 4> {
  ser_type serialize(const std::uint32_t& data) noexcept {
    ser_type buf;
    fmt_u32_write(buf.data(), data);
    return buf;
  }
};

template<>
struct DataTraits<std::uint64_t> final : ConstSizeDataTraits<std::uint64_t, 8> {
  ser_type serialize(const std::uint64_t& data) noexcept {
    ser_type buf;
    fmt_u64_write(buf.data(), data);
    return buf;
  }
};

template<class T, class Tr>
struct DataTraits<Written<T, Tr>> final : ConstSizeDataTraits<std::uint64_t, 8> {
  ser_type serialize(const Written<T, Tr>& data) noexcept {
    ser_type buf;
    fmt_u64_write(buf.data(), data.ptr());
    return buf;
  }
};


template<>
struct DataTraits<double> final : ConstSizeDataTraits<double, 8> {
  ser_type serialize(const double& data) noexcept {
    ser_type buf;
    fmt_f64_write(buf.data(), data);
    return buf;
  }
};

struct NullTerminatedString {
  static inline constexpr std::uint8_t alignment = 1;
  static std::size_t size(const std::string& v) noexcept { return v.size() + 1; }
  std::vector<char> serialize(const std::string& v) noexcept {
    std::vector<char> buf(v.begin(), v.end());
    buf.push_back('\0');
    buf.shrink_to_fit();
    return buf;
  }
  std::vector<char> serialize(const std::string_view& v) noexcept {
    std::vector<char> buf(v.begin(), v.end());
    buf.push_back('\0');
    buf.shrink_to_fit();
    return buf;
  }
};

}  // namespace hpctoolkit::formats

#endif // HPCTOOLKIT_PROFILE_FORMATS_PRIMITIVES_H
