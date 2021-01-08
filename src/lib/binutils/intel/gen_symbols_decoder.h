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

#ifndef PTI_SAMPLES_UTILS_GEN_SYMBOLS_DECODER_H_
#define PTI_SAMPLES_UTILS_GEN_SYMBOLS_DECODER_H_

#include <vector>

#ifdef ENABLE_IGC
#include <igc/ocl_igc_shared/executable_format/program_debug_data.h>
#endif // ENABLE_IGC


#include "elf_parser.h"

#define IS_POWER_OF_TWO(X) (!((X - 1)&X))
#define IGC_MAX_VALUE 1024

#ifdef ENABLE_IGC
class GenSymbolsDecoder {
 public:
  GenSymbolsDecoder(const std::vector<uint8_t>& symbols)
      : data_(symbols.data()), size_(symbols.size()) {}

  bool IsValid() const {
    return IsValidHeader();
  }

  std::vector<std::string> GetFileNames(
      const std::string& kernel_name) const {
    if (!IsValid()) {
      return std::vector<std::string>();
    }

    ElfParser parser = GetSection(kernel_name);
    return parser.GetFileNames();
  }

  LineInfo GetLineInfo(
      const std::string& kernel_name,
      uint32_t file_id) const {
    if (!IsValid()) {
      return LineInfo();
    }

    ElfParser parser = GetSection(kernel_name);
    return parser.GetLineInfo(file_id);
  }

 private:
  bool IsValidHeader() const {
    if (data_ == nullptr ||
        size_ < sizeof(iOpenCL::SProgramDebugDataHeaderIGC)) {
      return false;
    }

    const iOpenCL::SProgramDebugDataHeaderIGC* header =
      reinterpret_cast<const iOpenCL::SProgramDebugDataHeaderIGC*>(data_);
    return IS_POWER_OF_TWO(header->GPUPointerSizeInBytes) &&
           (header->GPUPointerSizeInBytes <= IGC_MAX_VALUE) &&
           (header->Device <= IGC_MAX_VALUE) &&
           (header->SteppingId <= IGC_MAX_VALUE) &&
           (header->NumberOfKernels <= IGC_MAX_VALUE);
  }

  ElfParser GetSection(const std::string& kernel_name) const {
    const uint8_t* ptr = data_;
    const iOpenCL::SProgramDebugDataHeaderIGC* header =
      reinterpret_cast<const iOpenCL::SProgramDebugDataHeaderIGC*>(ptr);
    ptr += sizeof(iOpenCL::SProgramDebugDataHeaderIGC);

    for (uint32_t i = 0; i < header->NumberOfKernels; ++i) {
      const iOpenCL::SKernelDebugDataHeaderIGC* kernel_header =
        reinterpret_cast<const iOpenCL::SKernelDebugDataHeaderIGC*>(ptr);
      ptr += sizeof(iOpenCL::SKernelDebugDataHeaderIGC);
      assert(ptr <= data_ + size_);

      const char* current_kernel_name = reinterpret_cast<const char*>(ptr);
      uint32_t aligned_kernel_name_size = sizeof(uint32_t) *
        (1 + (kernel_header->KernelNameSize - 1) / sizeof(uint32_t));
      ptr += aligned_kernel_name_size;
      assert(ptr <= data_ + size_);

      if (kernel_name == current_kernel_name) {
        assert(kernel_header->SizeGenIsaDbgInBytes == 0);

        ElfParser parser(ptr, kernel_header->SizeVisaDbgInBytes);
        if (!parser.IsValid()) {
          continue;
        }

        return parser;
      }

      ptr += kernel_header->SizeVisaDbgInBytes;
      assert(ptr <= data_ + size_);

      ptr += kernel_header->SizeGenIsaDbgInBytes;
      assert(ptr <= data_ + size_);
    }

    return ElfParser(nullptr, 0);
  }

  const uint8_t* data_ = nullptr;
  size_t size_ = 0;
};
#endif // ENABLE_IGC

#endif // PTI_SAMPLES_UTILS_GEN_SYMBOLS_DECODER_H_
