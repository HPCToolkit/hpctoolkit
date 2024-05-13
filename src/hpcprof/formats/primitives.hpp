// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#ifndef HPCTOOLKIT_PROFILE_FORMATS_PRIMITIVES_H
#define HPCTOOLKIT_PROFILE_FORMATS_PRIMITIVES_H

#include "core.hpp"

#include "../../common/lean/formats/primitive.h"

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
