//==============================================================
// Copyright © 2019 Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_SAMPLES_UTILS_GEN_BINARY_DECODER_H_
#define PTI_SAMPLES_UTILS_GEN_BINARY_DECODER_H_

#include <assert.h>

#include <vector>
#include <string>

#include <iga/kv.hpp>

#define MAX_STR_SIZE 1024

using InstructionList = std::vector< std::pair<uint32_t, std::string> >;

class GenBinaryDecoder {
 public:
  GenBinaryDecoder(const std::vector<uint8_t>& binary, iga_gen_t arch)
      : kernel_view_(arch, binary.data(), binary.size(),
                     iga::SWSB_ENCODE_MODE::SingleDistPipe) {}

  bool IsValid() const {
    return kernel_view_.decodeSucceeded();
  }

  InstructionList Disassemble() {
    if (!IsValid()) {
      return InstructionList();
    }

    InstructionList instruction_list;

    char text[MAX_STR_SIZE] = { 0 };
    int32_t offset = 0, size = 0;
    while (true) {
      size = kernel_view_.getInstSize(offset);
      if (size == 0) {
        break;
      }

      size_t lenght = kernel_view_.getInstSyntax(offset, text, MAX_STR_SIZE);
      assert(lenght > 0);
      instruction_list.push_back(std::make_pair(offset, text));

      offset += size;
    }

    return instruction_list;
  }

 private:
  KernelView kernel_view_;
};

#endif // PTI_SAMPLES_UTILS_GEN_BINARY_DECODER_H_
