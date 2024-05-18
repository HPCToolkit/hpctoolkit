//==============================================================
// SPDX-FileCopyrightText: 2019 Intel Corporation
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
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_SAMPLES_UTILS_ELF_PARSER_H_
#define PTI_SAMPLES_UTILS_ELF_PARSER_H_

#include <string.h>

#include <vector>

#include <elf.h>

#include "dwarf_parser.h"

class ElfParser {
 public:
  ElfParser(const uint8_t* data, uint32_t size) : data_(data), size_(size) {}

  bool IsValid() const {
    if (data_ == nullptr || size_ < sizeof(Elf64_Ehdr)) {
      return false;
    }

    const Elf64_Ehdr* header = reinterpret_cast<const Elf64_Ehdr*>(data_);
    if (memcmp(header->e_ident, ELFMAG, SELFMAG) != 0) {
      return false;
    }


    if (header->e_ident[4] != ELFCLASS64) {
      return false;
    }

    return true;
  }

  std::vector<std::string> GetFileNames() const {
    if (!IsValid()) {
      return std::vector<std::string>();
    }

    const uint8_t* section = nullptr;
    uint64_t section_size = 0;
    GetSection(".debug_line", &section, &section_size);
    if (section == nullptr || section_size == 0) {
      return std::vector<std::string>();
    }

    DwarfParser parser(section, static_cast<uint32_t>(section_size));
    if (!parser.IsValid()) {
      return std::vector<std::string>();
    }

    return parser.GetFileNames();
  }

  LineInfo GetLineInfo(uint32_t file_id) const {
    if (!IsValid()) {
      return LineInfo();
    }

    const uint8_t* section = nullptr;
    uint64_t section_size = 0;
    GetSection(".debug_line", &section, &section_size);
    if (section == nullptr || section_size == 0) {
      return LineInfo();
    }

    DwarfParser parser(section, static_cast<uint32_t>(section_size));
    if (!parser.IsValid()) {
      return LineInfo();
    }

    return parser.GetLineInfo(file_id);
  }

  std::vector<uint8_t> GetGenBinary() const {
    if (!IsValid()) {
      return std::vector<uint8_t>();
    }

    const uint8_t* section = nullptr;
    uint64_t section_size = 0;
    GetSection("Intel(R) OpenCL Device Binary", &section, &section_size);
    if (section == nullptr || section_size == 0) {
      return std::vector<uint8_t>();
    }

    std::vector<uint8_t> binary(section_size);
    memcpy(binary.data(), section, section_size);
    return binary;
  }

 private:
  void GetSection(const char* name,
                  const uint8_t** section,
                  uint64_t* section_size) const {
    if (section == nullptr || section_size == nullptr)
      std::abort();

    const Elf64_Ehdr* header = reinterpret_cast<const Elf64_Ehdr*>(data_);
    const Elf64_Shdr* section_header =
      reinterpret_cast<const Elf64_Shdr*>(data_ + header->e_shoff);
    const char* name_section = reinterpret_cast<const char*>(
        data_ + section_header[header->e_shstrndx].sh_offset);

    for (uint32_t i = 1; i < header->e_shnum; ++i) {
      const char* section_name = name_section + section_header[i].sh_name;
      if (strcmp(section_name, name) == 0) {
        *section = data_ + section_header[i].sh_offset;
        *section_size = section_header[i].sh_size;
        return;
      }
    }

    *section = nullptr;
    *section_size = 0;
  }

 private:
  const uint8_t* data_ = nullptr;
  uint32_t size_ = 0;
};

#endif // PTI_SAMPLES_UTILS_ELF_PARSER_H_
