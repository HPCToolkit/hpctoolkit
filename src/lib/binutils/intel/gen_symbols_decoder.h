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

#ifndef PTI_SAMPLES_UTILS_GEN_SYMBOLS_DECODER_H_
#define PTI_SAMPLES_UTILS_GEN_SYMBOLS_DECODER_H_

#include <vector>

#ifdef ENABLE_IGC
#include <igc/ocl_igc_shared/executable_format/program_debug_data.h>
#endif  // ENABLE_IGC

#include "elf_parser.h"

#define IS_POWER_OF_TWO(X) (!((X - 1) & X))
#define IGC_MAX_VALUE      1024

#ifdef ENABLE_IGC
class GenSymbolsDecoder {
public:
  GenSymbolsDecoder(const std::vector<uint8_t>& symbols)
      : data_(symbols.data()), size_(symbols.size()) {}

  bool IsValid() const { return IsValidHeader(); }

  std::vector<std::string> GetFileNames(const std::string& kernel_name) const {
    if (!IsValid()) {
      return std::vector<std::string>();
    }

    ElfParser parser = GetSection(kernel_name);
    return parser.GetFileNames();
  }

  LineInfo GetLineInfo(const std::string& kernel_name, uint32_t file_id) const {
    if (!IsValid()) {
      return LineInfo();
    }

    ElfParser parser = GetSection(kernel_name);
    return parser.GetLineInfo(file_id);
  }

private:
  bool IsValidHeader() const {
    if (data_ == nullptr || size_ < sizeof(iOpenCL::SProgramDebugDataHeaderIGC)) {
      return false;
    }

    const iOpenCL::SProgramDebugDataHeaderIGC* header =
        reinterpret_cast<const iOpenCL::SProgramDebugDataHeaderIGC*>(data_);
    return IS_POWER_OF_TWO(header->GPUPointerSizeInBytes)
        && (header->GPUPointerSizeInBytes <= IGC_MAX_VALUE) && (header->Device <= IGC_MAX_VALUE)
        && (header->SteppingId <= IGC_MAX_VALUE) && (header->NumberOfKernels <= IGC_MAX_VALUE);
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
      if (ptr > data_ + size_)
        std::abort();

      const char* current_kernel_name = reinterpret_cast<const char*>(ptr);
      uint32_t aligned_kernel_name_size =
          sizeof(uint32_t) * (1 + (kernel_header->KernelNameSize - 1) / sizeof(uint32_t));
      ptr += aligned_kernel_name_size;
      if (ptr > data_ + size_)
        std::abort();

      if (kernel_name == current_kernel_name) {
        if (kernel_header->SizeGenIsaDbgInBytes != 0)
          std::abort();

        ElfParser parser(ptr, kernel_header->SizeVisaDbgInBytes);
        if (!parser.IsValid()) {
          continue;
        }

        return parser;
      }

      ptr += kernel_header->SizeVisaDbgInBytes;
      if (ptr > data_ + size_)
        std::abort();

      ptr += kernel_header->SizeGenIsaDbgInBytes;
      if (ptr > data_ + size_)
        std::abort();
    }

    return ElfParser(nullptr, 0);
  }

  const uint8_t* data_ = nullptr;
  size_t size_ = 0;
};
#endif  // ENABLE_IGC

#endif  // PTI_SAMPLES_UTILS_GEN_SYMBOLS_DECODER_H_
