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

#ifndef PTI_SAMPLES_UTILS_GEN_BINARY_DECODER_H_
#define PTI_SAMPLES_UTILS_GEN_BINARY_DECODER_H_

#include <assert.h>

#include <vector>
#include <string>



#ifdef ENABLE_IGC
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

#endif // ENABLE_IGC

#endif // PTI_SAMPLES_UTILS_GEN_BINARY_DECODER_H_
