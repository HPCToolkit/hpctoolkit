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

#ifndef PTI_SAMPLES_UTILS_IGC_BINARY_DECODER_H_
#define PTI_SAMPLES_UTILS_IGC_BINARY_DECODER_H_

#include <memory.h>

#ifdef ENABLE_IGC
#ifndef IGC_INCLUDE_FULL_PATH
#include <patch_list.h>
#else
#include <igc/ocl_igc_shared/executable_format/patch_list.h>
#endif

#include "gen_binary_decoder.h"

typedef enum {
    IGFX_UNKNOWN_CORE    = 0,
    IGFX_GEN3_CORE       = 1,   //Gen3 Family
    IGFX_GEN3_5_CORE     = 2,   //Gen3.5 Family
    IGFX_GEN4_CORE       = 3,   //Gen4 Family
    IGFX_GEN4_5_CORE     = 4,   //Gen4.5 Family
    IGFX_GEN5_CORE       = 5,   //Gen5 Family
    IGFX_GEN5_5_CORE     = 6,   //Gen5.5 Family
    IGFX_GEN5_75_CORE    = 7,   //Gen5.75 Family
    IGFX_GEN6_CORE       = 8,   //Gen6 Family
    IGFX_GEN7_CORE       = 9,   //Gen7 Family
    IGFX_GEN7_5_CORE     = 10,  //Gen7.5 Family
    IGFX_GEN8_CORE       = 11,  //Gen8 Family
    IGFX_GEN9_CORE       = 12,  //Gen9 Family
    IGFX_GEN10_CORE      = 13,  //Gen10 Family
    IGFX_GEN10LP_CORE    = 14,  //Gen10 LP Family
    IGFX_GEN11_CORE      = 15,  //Gen11 Family
    IGFX_GEN11LP_CORE    = 16,  //Gen11 LP Family
    IGFX_GEN12_CORE      = 17,  //Gen12 Family
    IGFX_GEN12LP_CORE    = 18,  //Gen12 LP Family
    IGFX_XE_HP_CORE      = 0x0c05,  //XE_HP family
    IGFX_XE_HPG_CORE     = 0x0c07,  // XE_HPG Family
    IGFX_XE_HPC_CORE     = 0x0c08,  // XE_HPC Family
    IGFX_MAX_CORE,              //Max Family, for lookup table

    IGFX_GENNEXT_CORE          = 0x7ffffffe,  //GenNext
    GFXCORE_FAMILY_FORCE_ULONG = 0x7fffffff
} GFXCORE_FAMILY;


using namespace iOpenCL;

class IgcBinaryDecoder {
 public:
  IgcBinaryDecoder(const std::vector<uint8_t>& binary) : binary_(binary) {}

  InstructionList Disassemble(const std::string& kernel_name) {
    if (!IsValidHeader()) {
      return InstructionList();
    }

    const SProgramBinaryHeader* header =
      reinterpret_cast<const SProgramBinaryHeader*>(binary_.data());
    iga_gen_t arch = GetArch(header->Device);
    if (arch == IGA_GEN_INVALID) {
      return InstructionList();
    }

    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(header) +
      sizeof(SProgramBinaryHeader) + header->PatchListSize;
    for (uint32_t i = 0; i < header->NumberOfKernels; ++i) {
      const SKernelBinaryHeaderCommon* kernel_header =
        reinterpret_cast<const SKernelBinaryHeaderCommon*>(ptr);

      ptr += sizeof(SKernelBinaryHeaderCommon);
      const char* name = (const char*)ptr;

      ptr += kernel_header->KernelNameSize;
      if (kernel_name == name) {
        std::vector<uint8_t> raw_binary(kernel_header->KernelHeapSize);
        memcpy(raw_binary.data(), ptr,
               kernel_header->KernelHeapSize * sizeof(uint8_t));
        GenBinaryDecoder decoder(raw_binary, arch);
        return decoder.Disassemble();
      }

      ptr += kernel_header->PatchListSize +
        kernel_header->KernelHeapSize +
        kernel_header->GeneralStateHeapSize +
        kernel_header->DynamicStateHeapSize +
        kernel_header->SurfaceStateHeapSize;
    }

    return InstructionList();
  }

 private:
  bool IsValidHeader() {
    if (binary_.size() < sizeof(SProgramBinaryHeader)) {
      return false;
    }

    const SProgramBinaryHeader* header =
      reinterpret_cast<const SProgramBinaryHeader*>(binary_.data());
    if (header->Magic != MAGIC_CL) {
      return false;
    }

    return true;
  }

  static iga_gen_t GetArch(uint32_t device) {
    switch (device) {
    case IGFX_GEN8_CORE:
      return IGA_GEN8;
    case IGFX_GEN9_CORE:
      return IGA_GEN9p5;
    case IGFX_GEN11_CORE:
    case IGFX_GEN11LP_CORE:
      return IGA_GEN11;
    case IGFX_GEN12_CORE:
    case IGFX_GEN12LP_CORE:
      return IGA_GEN12p1;
    case IGFX_XE_HP_CORE:
      return IGA_XE_HP;
    case IGFX_XE_HPG_CORE:
      return IGA_XE_HPG;
    case IGFX_XE_HPC_CORE:
      return IGA_XE_HPC;
    default:
      break;
    }
    return IGA_GEN_INVALID;
  }

private:
    std::vector<uint8_t> binary_;
};
#endif // ENABLE_IGC

#endif
