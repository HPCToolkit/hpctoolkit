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

#ifndef PTI_SAMPLES_UTILS_DWARF_PARSER_H_
#define PTI_SAMPLES_UTILS_DWARF_PARSER_H_

#include <assert.h>
#include <string.h>

#include <string>
#include <vector>

#include "dwarf_state_machine.h"

using LineInfo = std::vector< std::pair<uint32_t, uint32_t> >;

class DwarfParser {
 public:
  DwarfParser(const uint8_t* data, uint32_t size) : data_(data), size_(size) {}

  bool IsValid() const {
    if (data_ == nullptr || size_ < sizeof(Dwarf32Header)) {
      return false;
    }

    const Dwarf32Header* header =
      reinterpret_cast<const Dwarf32Header*>(data_);
    if (header->version != DWARF_VERSION) {
      return false;
    }

    return true;
  }

  std::vector<std::string> GetFileNames() const {
    if (!IsValid()) {
      return std::vector<std::string>();
    }
    std::vector<std::string> file_names;
    ProcessHeader(&file_names);
    return file_names;
  }

  LineInfo GetLineInfo(uint32_t file_id) const {
    if (file_id == 0)
      std::abort();

    if (!IsValid()) {
      return LineInfo();
    }

    const uint8_t* ptr = ProcessHeader(nullptr);
    if (ptr >= data_ + size_)
      std::abort();
    const uint8_t* line_number_program = ptr;
    uint32_t line_number_program_size =
      static_cast<uint32_t>(data_ + size_ - line_number_program);
    DwarfLineInfo line_info =
      DwarfStateMachine(line_number_program, line_number_program_size,
                        reinterpret_cast<const Dwarf32Header*>(data_)).Run();
    if (line_info.size() == 0) {
      return LineInfo();
    }

    return ProcessLineInfo(line_info, file_id);
  }

 private:
  const uint8_t* ProcessHeader(std::vector<std::string>* file_names) const {
    const uint8_t* ptr = data_;
    const Dwarf32Header* header = reinterpret_cast<const Dwarf32Header*>(ptr);

    ptr += sizeof(Dwarf32Header);

    // standard_opcode_lengths
    for (uint8_t i = 1; i < header->opcode_base; ++i) {
      uint32_t value = 0;
      bool done = false;
      ptr = utils::leb128::Decode32(ptr, value, done);
      if (!done)
        std::abort();
    }

    // include_directories
    while (*ptr != 0) {
      const char* include_directory = reinterpret_cast<const char*>(ptr);
      ptr += strlen(include_directory) + 1;
    }
    ++ptr;

    // file_names
    if (*ptr == 0)
      std::abort();
    while (*ptr != 0) {
      std::string file_name(reinterpret_cast<const char*>(ptr));
      ptr += file_name.size() + 1;

      bool done = false;
      uint32_t directory_index = 0;
      ptr = utils::leb128::Decode32(ptr, directory_index, done);
      if (!done) std::abort();

      uint32_t time = 0;
      ptr = utils::leb128::Decode32(ptr, time, done);
      if (!done) std::abort();

      uint32_t size = 0;
      ptr = utils::leb128::Decode32(ptr, size, done);
      if (!done) std::abort();

      if (file_names != nullptr) {
        file_names->push_back(file_name);
      }
    }

    ++ptr;
    return ptr;
  }

  LineInfo ProcessLineInfo(DwarfLineInfo line_info, uint32_t file) const {
    LineInfo result;

    uint32_t address = 0;
    uint32_t line = 0;
    for (auto item : line_info) {
      if (address > item.first)
        std::abort();  // DWARF lines not in sorted order?
      if (item.second.first != file) {
        continue;
      }
      if (line == item.second.second) {
        continue;
      }
      address = (uint32_t)item.first;
      line = item.second.second;
      result.push_back(std::make_pair(address, line));
    }

    return result;
}

  const uint8_t* data_;
  uint32_t size_;
};

#endif // PTI_SAMPLES_UTILS_DWARF_PARSER_H_
