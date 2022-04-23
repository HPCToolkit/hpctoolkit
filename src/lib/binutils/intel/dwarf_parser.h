// -*-Mode: C++;-*- // technically C99

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
// Copyright ((c)) 2002-2022, Rice University
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

#include "dwarf_state_machine.h"

#include <assert.h>
#include <string.h>
#include <string>
#include <vector>

using LineInfo = std::vector<std::pair<uint32_t, uint32_t>>;

class DwarfParser {
public:
  DwarfParser(const uint8_t* data, uint32_t size) : data_(data), size_(size) {}

  bool IsValid() const {
    if (data_ == nullptr || size_ < sizeof(Dwarf32Header)) {
      return false;
    }

    const Dwarf32Header* header = reinterpret_cast<const Dwarf32Header*>(data_);
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
    assert(file_id > 0);

    if (!IsValid()) {
      return LineInfo();
    }

    const uint8_t* ptr = ProcessHeader(nullptr);
    assert(ptr < data_ + size_);
    const uint8_t* line_number_program = ptr;
    uint32_t line_number_program_size = static_cast<uint32_t>(data_ + size_ - line_number_program);
    DwarfLineInfo line_info = DwarfStateMachine(
                                  line_number_program, line_number_program_size,
                                  reinterpret_cast<const Dwarf32Header*>(data_))
                                  .Run();
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
      assert(done);
    }

    // include_directories
    while (*ptr != 0) {
      const char* include_directory = reinterpret_cast<const char*>(ptr);
      ptr += strlen(include_directory) + 1;
    }
    ++ptr;

    // file_names
    assert(*ptr != 0);
    while (*ptr != 0) {
      std::string file_name(reinterpret_cast<const char*>(ptr));
      ptr += file_name.size() + 1;

      bool done = false;
      uint32_t directory_index = 0;
      ptr = utils::leb128::Decode32(ptr, directory_index, done);
      assert(done);

      uint32_t time = 0;
      ptr = utils::leb128::Decode32(ptr, time, done);
      assert(done);

      uint32_t size = 0;
      ptr = utils::leb128::Decode32(ptr, size, done);
      assert(done);

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
      assert(address <= item.first);
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

#endif  // PTI_SAMPLES_UTILS_DWARF_PARSER_H_
