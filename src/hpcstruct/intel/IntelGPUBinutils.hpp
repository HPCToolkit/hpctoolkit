// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

//******************************************************************************
// type definitions
//*****************************************************************************


#ifndef BINUTILS_INTEL_INTEL_GPU_BINUTILS
#define BINUTILS_INTEL_INTEL_GPU_BINUTILS

enum SHT_OPENCL : uint32_t {
    SHT_OPENCL_SOURCE = 0xff000000,                  // CL source to link into LLVM binary
    SHT_OPENCL_HEADER = 0xff000001,                  // CL header to link into LLVM binary
    SHT_OPENCL_LLVM_TEXT = 0xff000002,               // LLVM text
    SHT_OPENCL_LLVM_BINARY = 0xff000003,             // LLVM byte code
    SHT_OPENCL_LLVM_ARCHIVE = 0xff000004,            // LLVM archives(s)
    SHT_OPENCL_DEV_BINARY = 0xff000005,              // Device binary (coherent by default)
    SHT_OPENCL_OPTIONS = 0xff000006,                 // CL Options
    SHT_OPENCL_PCH = 0xff000007,                     // PCH (pre-compiled headers)
    SHT_OPENCL_DEV_DEBUG = 0xff000008,               // Device debug
    SHT_OPENCL_SPIRV = 0xff000009,                   // SPIRV
    SHT_OPENCL_NON_COHERENT_DEV_BINARY = 0xff00000a, // Non-coherent Device binary
    SHT_OPENCL_SPIRV_SC_IDS = 0xff00000b,            // Specialization Constants IDs
    SHT_OPENCL_SPIRV_SC_VALUES = 0xff00000c          // Specialization Constants values
};

//******************************************************************************
// interface functions
//******************************************************************************

bool
findIntelGPUBins
(
 const std::string &file_name,
 const char *file_buffer,
 size_t file_size,
 ElfFileVector *filevector
);

#endif
