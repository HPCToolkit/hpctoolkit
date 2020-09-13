//==============================================================
// Copyright © 2019 Intel Corporation
//
// SPDX-License-Identifier: MIT
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