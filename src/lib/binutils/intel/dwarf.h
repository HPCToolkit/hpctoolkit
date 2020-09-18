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

#ifndef PTI_SAMPLES_UTILS_DWARF_H_
#define PTI_SAMPLES_UTILS_DWARF_H_

#include <stdint.h>

#define DWARF_VERSION 4

#define DW_LNS_COPY             0x01
#define DW_LNS_ADVANCE_PC       0x02
#define DW_LNS_ADVANCE_LINE     0x03
#define DW_LNS_SET_FILE         0x04
#define DW_LNS_SET_COLUMN       0x05
#define DW_LNS_NEGATE_STMT      0x06
#define DW_LNS_SET_BASIC_BLOCK  0x07
#define DW_LNS_CONST_ADD_PC     0x08
#define DW_LNS_FIXED_ADVANCE_PC 0x09

#define DW_LNS_END_SEQUENCE     0x01
#define DW_LNE_SET_ADDRESS      0x02

#pragma pack(push, 1)
struct Dwarf32Header {
  uint32_t unit_length;
  uint16_t version;
  uint32_t header_length;
  uint8_t  minimum_instruction_length;
  uint8_t  maximum_operations_per_instruction;
  uint8_t  default_is_stmt;
  int8_t   line_base;
  uint8_t  line_range;
  uint8_t  opcode_base;
};
#pragma pack(pop)

#endif // PTI_SAMPLES_UTILS_DWARF_H_
