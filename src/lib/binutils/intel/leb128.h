//==============================================================
// Copyright © 2019 Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_SAMPLES_UTILS_LEB128_H_
#define PTI_SAMPLES_UTILS_LEB128_H_

#include <stdint.h>

namespace utils {
namespace leb128 {

inline const uint8_t* Decode32(const uint8_t* ptr, uint32_t& value,
                               bool& done) {
  uint8_t byte = 0;
  uint8_t count = 0;
  uint8_t shift = 0;

  value = 0;
  done = false;

  while (count < sizeof(uint32_t)) {
    byte = *ptr;
    value |= ((byte & 0x7F) << shift);
    shift += 7;

    ++ptr;
    ++count;

    if ((byte & 0x80) == 0) {
      done = true;
      break;
    }
  }

  return ptr;
}

inline const uint8_t* Decode32(const uint8_t* ptr, int32_t& value,
                               bool& done) {
  uint8_t byte = 0;
  uint8_t count = 0;
  uint8_t shift = 0;

  value = 0;
  done = false;

  while (count < sizeof(int32_t)) {
    byte = *ptr;
    value |= ((byte & 0x7F) << shift);
    shift += 7;

    ++ptr;
    ++count;

    if ((byte & 0x80) == 0) {
      done = true;
      break;
    }
  }

  if ((shift < 8 * sizeof(int32_t)) && ((byte & 0x40) > 0)) {
    value |= -(1u << shift);
  }

  return ptr;
}

} // namespace leb128
} // namespace utils

#endif // PTI_SAMPLES_UTILS_LEB128_H_