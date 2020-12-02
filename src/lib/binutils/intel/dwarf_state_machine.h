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


#ifndef PTI_SAMPLES_UTILS_DWARF_STATE_MACHINE_H_
#define PTI_SAMPLES_UTILS_DWARF_STATE_MACHINE_H_

#include "dwarf.h"
#include "leb128.h"

using DwarfLineInfo =
  std::vector< std::pair<uint64_t, std::pair<uint32_t, uint32_t> > >;

struct DwarfState {
  uint64_t address;
  uint32_t operation;
  uint32_t line;
  uint32_t file;
};

class DwarfStateMachine {
 public:
  DwarfStateMachine(const uint8_t* data, uint32_t size,
                    const Dwarf32Header* header)
      : data_(data), size_(size), header_(header) {
    assert(data_ != nullptr);
    assert(size_ > 0);
    assert(header_ != nullptr);
  }

  DwarfLineInfo Run() {
    const uint8_t* ptr = data_;
    const uint8_t* end = ptr + size_;

    while (ptr < end) {
      if (*ptr == 0) {
        ptr = RunExtended(ptr);
      } else if (*ptr < header_->opcode_base) {
        ptr = RunStandard(ptr);
      } else {
        ptr = RunSpecial(ptr);
      }
    }

    return line_info_;
  }

 private:
  const uint8_t* RunSpecial(const uint8_t* ptr) {
    assert(*ptr >= header_->opcode_base);

    uint8_t adjusted_opcode = (*ptr) - header_->opcode_base;
    uint8_t operation_advance = adjusted_opcode / header_->line_range;
    UpdateAddress(operation_advance);
    UpdateOperation(operation_advance);
    UpdateLine(adjusted_opcode);

    UpdateLineInfo();

    ++ptr;
    assert(ptr < data_ + size_);
    return ptr;
  }

  const uint8_t* RunStandard(const uint8_t* ptr) {
    uint8_t opcode = *ptr;
    ++ptr;

    assert(opcode < header_->opcode_base);
    assert(ptr < data_ + size_);

    switch (opcode) {
      case DW_LNS_COPY: {
        UpdateLineInfo();
        break;
      }
      case DW_LNS_ADVANCE_PC: {
        uint32_t operation_advance = 0;
        bool done = false;
        ptr = utils::leb128::Decode32(ptr, operation_advance, done);
        assert(done);
        assert(ptr < data_ + size_);
        UpdateAddress(operation_advance);
        UpdateOperation(operation_advance);
        break;
      }
      case DW_LNS_ADVANCE_LINE: {
        int32_t line = 0;
        bool done = false;
        ptr = utils::leb128::Decode32(ptr, line, done);
        assert(done);
        assert(ptr < data_ + size_);
        state_.line += line;
        break;
      }
      case DW_LNS_SET_FILE: {
        uint32_t file = 0;
        bool done = false;
        ptr = utils::leb128::Decode32(ptr, file, done);
        assert(done);
        assert(ptr < data_ + size_);
        state_.file = file;
        break;
      }
      case DW_LNS_SET_COLUMN: {
        uint32_t column = 0;
        bool done = false;
        ptr = utils::leb128::Decode32(ptr, column, done);
        assert(done);
        assert(ptr < data_ + size_);
        break;
      }
      case DW_LNS_NEGATE_STMT:
      case DW_LNS_SET_BASIC_BLOCK:
          break;
      case DW_LNS_CONST_ADD_PC: {
        uint8_t adjusted_opcode = 255 - header_->opcode_base;
        uint8_t operation_advance = adjusted_opcode / header_->line_range;
        UpdateAddress(operation_advance);
        UpdateOperation(operation_advance);
        break;
      }
      case DW_LNS_FIXED_ADVANCE_PC: {
        uint16_t advance = *((uint16_t*)ptr);
        ptr += sizeof(uint16_t);
        assert(ptr < data_ + size_);
        state_.address += advance;
        state_.operation = 0;
        break;
      }
      default: {
        assert(0); // Not supported
        break;
      }
    }

    return ptr;
  }

  const uint8_t* RunExtended(const uint8_t* ptr) {
    assert(*ptr == 0);
    ++ptr;
    assert(ptr < data_ + size_);

    uint8_t size = *ptr;
    assert(size > 0);
    ++ptr;
    assert(ptr < data_ + size_);

    uint8_t opcode = *ptr;
    ++ptr;
    assert(ptr <= data_ + size_);

    switch (opcode) {
      case DW_LNS_END_SEQUENCE: {
        assert(ptr == data_ + size_);
        UpdateLineInfo();
        break;
      }
      case DW_LNE_SET_ADDRESS: {
        uint64_t address = *((const uint64_t*)ptr);
        assert(size - 1 == sizeof(uint64_t));
        ptr += sizeof(uint64_t);
        assert(ptr < data_ + size_);
        state_.address = address;
        break;
      }
      default: {
        assert(0); // Not supported
        break;
      }
    }

    return ptr;
  }

  void UpdateAddress(uint32_t operation_advance) {
    state_.address += header_->minimum_instruction_length *
                      ((state_.operation + operation_advance) /
                      header_->maximum_operations_per_instruction);
  }

  void UpdateOperation(uint32_t operation_advance) {
    state_.operation = (state_.operation + operation_advance) %
                       header_->maximum_operations_per_instruction;
  }

  void UpdateLine(uint32_t adjusted_opcode) {
    state_.line += header_->line_base +
                   (adjusted_opcode % header_->line_range);
  }

  void UpdateLineInfo() {
    line_info_.push_back(
      std::make_pair(state_.address,
                     std::make_pair(state_.file, state_.line)));
  }

private:
  const uint8_t* data_ = nullptr;
  uint32_t size_ = 0;    
  const Dwarf32Header* header_ = nullptr;

  DwarfState state_ = { 0, 0, 1, 1 };
  DwarfLineInfo line_info_;
};

#endif // PTI_SAMPLES_UTILS_DWARF_STATE_MACHINE_H_
