//==============================================================
// Copyright © 2019 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
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
