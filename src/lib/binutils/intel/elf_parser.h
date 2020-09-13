//==============================================================
// Copyright © 2019 Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_SAMPLES_UTILS_ELF_PARSER_H_
#define PTI_SAMPLES_UTILS_ELF_PARSER_H_

#include <string.h>

#include <vector>

#include "elf.h"
#include "dwarf_parser.h"

class ElfParser {
 public:
  ElfParser(const uint8_t* data, uint32_t size) : data_(data), size_(size) {}

  bool IsValid() const {
    if (data_ == nullptr || size_ < sizeof(Elf64Header)) {
      return false;
    }

    const Elf64Header* header = reinterpret_cast<const Elf64Header*>(data_);
    if (header->ident[0] != ELF_MAGIC_NUMBER ||
        header->ident[1] != 'E' ||
        header->ident[2] != 'L' ||
        header->ident[3] != 'F') {
      return false;
    }


    if (header->ident[4] != 2) { // 64-bit format
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
    assert(section != nullptr && section_size != nullptr);

    const Elf64Header* header = reinterpret_cast<const Elf64Header*>(data_);
    const Elf64SectionHeader* section_header =
      reinterpret_cast<const Elf64SectionHeader*>(data_ + header->shoff);
    const char* name_section = reinterpret_cast<const char*>(
        data_ + section_header[header->shstrndx].offset);
    
    for (uint32_t i = 1; i < header->shnum; ++i) {
      const char* section_name = name_section + section_header[i].name;
      if (strcmp(section_name, name) == 0) {
        *section = data_ + section_header[i].offset;
        *section_size = section_header[i].size;
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