//==============================================================
// Copyright © 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#pragma once

#include <memory.h>

#include <igc/ocl_igc_shared/executable_format/patch_list.h>

#include <metrics_discovery_internal_api.h>

#include "gen_binary_decoder.h"

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
    switch (1 << device) {
      case MetricsDiscovery::PLATFORM_BDW:
        return IGA_GEN8;
      case MetricsDiscovery::PLATFORM_SKL:
        return IGA_GEN9;
      case MetricsDiscovery::PLATFORM_KBL:
        return IGA_GEN9p5;
      case MetricsDiscovery::PLATFORM_ICL:
        return IGA_GEN11;
      case 1 << 18: // TGL (?)
        return IGA_GEN12p1;
      default:
        break;
    }
    return IGA_GEN_INVALID;
  }

private:
    std::vector<uint8_t> binary_;
};
