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
// Copyright ((c)) 2002-2020, Rice University
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


//***************************************************************************

//******************************************************************************
// system includes
//******************************************************************************

#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <libelf.h>

#include "igc_binary_decoder.h"
#include "gen_symbols_decoder.h"



//******************************************************************************
// local includes
//******************************************************************************

#include <lib/binutils/ElfHelper.hpp>
#include <lib/support/diagnostics.h>
#include <lib/support/RealPathMgr.cpp>
#include "IntelGPUBinutils.hpp"


//******************************************************************************
// macros
//******************************************************************************

#define DBG 1

#define INTEL_GPU_DEBUG_SECTION_NAME "Intel(R) OpenCL Device Debug"



//******************************************************************************
// private operations
//******************************************************************************

static __attribute__((unused)) const char *
opencl_elf_section_type
(
  Elf64_Word sh_type
)
{
  switch (sh_type) {
    case SHT_OPENCL_SOURCE:
      return "SHT_OPENCL_SOURCE";
    case SHT_OPENCL_HEADER:
      return "SHT_OPENCL_HEADER";
    case SHT_OPENCL_LLVM_TEXT:
      return "SHT_OPENCL_LLVM_TEXT";
    case SHT_OPENCL_LLVM_BINARY:
      return "SHT_OPENCL_LLVM_BINARY";
    case SHT_OPENCL_LLVM_ARCHIVE:
      return "SHT_OPENCL_LLVM_ARCHIVE";
    case SHT_OPENCL_DEV_BINARY:
      return "SHT_OPENCL_DEV_BINARY";
    case SHT_OPENCL_OPTIONS:
      return "SHT_OPENCL_OPTIONS";
    case SHT_OPENCL_PCH:
      return "SHT_OPENCL_PCH";
    case SHT_OPENCL_DEV_DEBUG:
      return "SHT_OPENCL_DEV_DEBUG";
    case SHT_OPENCL_SPIRV:
      return "SHT_OPENCL_SPIRV";
    case SHT_OPENCL_NON_COHERENT_DEV_BINARY:
      return "SHT_OPENCL_NON_COHERENT_DEV_BINARY";
    case SHT_OPENCL_SPIRV_SC_IDS:
      return "SHT_OPENCL_SPIRV_SC_IDS";
    case SHT_OPENCL_SPIRV_SC_VALUES:
      return "SHT_OPENCL_SPIRV_SC_VALUES";
    default:
      return "unknown type";
  }
}


#define MAX_STR_SIZE 1024
#define INDENT  "  "

std::map<int32_t, bool> visitedBlockOffsets;



//******************************************************************************
// private operations
//******************************************************************************

class Edge {
  public:
    int32_t from; 
    int32_t to;
    int32_t from_blockEndOffset;

    Edge(int32_t f, int32_t t, int32_t from_b) {
      from = f;
      to = t;
      from_blockEndOffset = from_b;
    }

    bool operator == (const Edge &that) const 
    {
      return((this->from == that.from) && (this->to == that.to));
    }
    
    bool operator<(const Edge& that) const 
    {
      if (this->from == that.from) {
        return (this->to < that.to);
      } else {
        return (this->from < that.from);
      }
    }
};


static std::set<Edge>
get_cfg_edges
(
  std::vector<uint8_t> &binary
)
{
  KernelView kv(IGA_GEN9, binary.data(), binary.size(),
      iga::SWSB_ENCODE_MODE::SingleDistPipe);
  size_t binary_size = binary.size();
  std::set<Edge> cfg_edges;

  std::vector<int32_t> block_offsets;
  int32_t offset = 0;
  int32_t size;
  while (offset < binary_size) {
    int32_t prev_block_start_offset;
    int32_t prev_block_end_offset;
    int32_t block_start_offset;
    bool isStartOfBasicBlock = kv.isInstTarget(offset);
    if (isStartOfBasicBlock) {
      block_offsets.push_back(offset);
      visitedBlockOffsets.insert({offset, false});
      block_start_offset = offset;  
    }
    size = kv.getInstSize(offset);
    while (!kv.isInstTarget(offset + size) && (offset + size < binary_size)) {
      offset += size; 
      size = kv.getInstSize(offset);
      if (size == 0) {
        // this is a weird edge case, what to do?
        break;
      }
    }

    int32_t *jump_targets = new int32_t[KV_MAX_TARGETS_PER_INSTRUCTION];
    size_t jump_targets_count = kv.getInstTargets(offset, jump_targets);
    int32_t next_block_start_offset = offset + size;
    bool isFallThroughEdgeAdded = false;

    for (size_t i = 0; i < jump_targets_count; i++) {
      if (jump_targets[i] == next_block_start_offset) {
        isFallThroughEdgeAdded = true;
      } else if (jump_targets[i] == block_start_offset) {
        if (block_offsets.size() >= 2) {
          int32_t from = block_offsets[block_offsets.size() - 2];
          int32_t from_blockEndOffset;
          for (Edge edge: cfg_edges) {
            if (edge.from == from && edge.to == block_start_offset) {
              from_blockEndOffset  = edge.from_blockEndOffset;
            }
          } 
          cfg_edges.insert(Edge(block_offsets[block_offsets.size() - 2], block_start_offset, from_blockEndOffset));
        }
      }
      cfg_edges.insert(Edge(block_start_offset, jump_targets[i], next_block_start_offset - size));
    }
    if(!isFallThroughEdgeAdded) {
      cfg_edges.insert(Edge(block_start_offset, next_block_start_offset, next_block_start_offset - size));
    }
    prev_block_start_offset = block_start_offset;
    prev_block_end_offset = offset; 
    offset += size;
  }
  cfg_edges.insert(Edge(block_offsets[block_offsets.size() - 1], binary_size, binary_size - size));
  return cfg_edges;
}


static void
printCFGEdges
(
  std::set<Edge> &cfg_edges
)
{
  for (Edge edge: cfg_edges) {
    std::cout << edge.from << "->" << edge.to << std::endl; 
  } 
}


static void
printBasicBlocks
(
  std::vector<uint8_t> &binary,
  std::set<Edge> &cfg_edges
)
{
  KernelView kv(IGA_GEN9, binary.data(), binary.size(), iga::SWSB_ENCODE_MODE::SingleDistPipe);
  int32_t offset;
  char text[MAX_STR_SIZE] = { 0 };
  size_t length;
  int32_t size;

  for (Edge edge: cfg_edges) {
    offset = edge.from;
    if(edge.from == edge.to) {
      // skip self-loops
      continue;
    }
    auto it = visitedBlockOffsets.find(offset);
    if (it->second) {
      continue;
    } else {
      it->second = true;
    }
    std::cout << offset << " [ label=\"\\\n"; 
    while (offset < edge.to) {
      size = kv.getInstSize(offset);
      length = kv.getInstSyntax(offset, text, MAX_STR_SIZE);
      assert(length > 0);
      std::cout << offset << ": " << text << "\\\l";
      offset += size;
    }
    std::cout << "\" shape=\"box\"]; \n" << std::endl;
  } 
}


static std::vector<int32_t>
getBlockOffsets
(
  std::vector<uint8_t> &binary
)
{
  std::vector<int32_t> block_offsets;
  int32_t offset = 0;
  int32_t size = 0;
  KernelView kv(IGA_GEN9, binary.data(), binary.size(),
      iga::SWSB_ENCODE_MODE::SingleDistPipe);

  while (offset < binary.size()) {
    bool isStartOfBasicBlock = kv.isInstTarget(offset);
    if (isStartOfBasicBlock) {
      block_offsets.push_back(offset);
    }
    size = kv.getInstSize(offset);
    offset += size; 
  }
  return block_offsets;
}


static void
doIndent(std::stringstream *ss, int depth)
{
  for (int n = 1; n <= depth; n++) {
    *ss << INDENT;
  }
}



//******************************************************************************
// interface operations
//******************************************************************************

// pass Intel kernel's raw gen binary
// kernel's text region is a raw gen binary
// you  can find kernel nested in [debug section of GPU binary/separate debug section dump]
void
printCFGInDotGraph
(
  std::vector<uint8_t> intelRawGenBinary
)
{
  std::cout << "digraph GEMM_iga {" << std::endl;
  std::set<Edge> edges = get_cfg_edges(intelRawGenBinary);
  printBasicBlocks(intelRawGenBinary, edges);
  printCFGEdges(edges);
  std::cout << "}" << std::endl;
}


std::string
getBlockAndInstructionOffsets
(
 std::vector<uint8_t> &intelRawGenBinary
)
{
  std::stringstream ss;
  std::vector<int32_t> block_offsets = getBlockOffsets(intelRawGenBinary);
  KernelView kv(IGA_GEN9, intelRawGenBinary.data(), intelRawGenBinary.size(),
      iga::SWSB_ENCODE_MODE::SingleDistPipe);
  int32_t offset, size;

  for(auto i = 0; i < block_offsets.size()-1; i++) {
    offset = block_offsets[i];
    doIndent(&ss, 1);
    ss << "<B o=\"0x" << std::hex << offset << "\">\n"; 
    doIndent(&ss, 2);
    while (offset != block_offsets[i+1]) {
      ss << "<I o=\"0x" << std::hex << offset << "\"/>";
      size = kv.getInstSize(offset);
      offset += size;
    }
    ss << "\n"; 
    doIndent(&ss, 1);
    ss << "</B>\n"; 
  }
  return ss.str();
}


//******************************************************************************
// interface operations
//******************************************************************************

bool
findIntelGPUBins
(
 const std::string &file_name,
 const char *file_buffer,
 size_t file_size,
 ElfFileVector *filevector
)
{
  const char *ptr = file_buffer;
  const SProgramDebugDataHeaderIGC* header =
    reinterpret_cast<const SProgramDebugDataHeaderIGC*>(ptr);
  ptr += sizeof(SProgramDebugDataHeaderIGC);

  if (header->NumberOfKernels == 0) {
    return false;
  }

  auto iter = file_name.rfind("/");
  if (iter == std::string::npos) {
    return false;
  }
  std::string dir_name = file_name.substr(0, iter + 1);
  
  for (uint32_t i = 0; i < header->NumberOfKernels; ++i) {
    const SKernelDebugDataHeaderIGC *kernel_header =
      reinterpret_cast<const SKernelDebugDataHeaderIGC*>(ptr);
    ptr += sizeof(SKernelDebugDataHeaderIGC);
    std::string kernel_name(ptr);

    unsigned kernel_name_size_aligned = sizeof(uint32_t) *
      (1 + (kernel_header->KernelNameSize - 1) / sizeof(uint32_t));
    ptr += kernel_name_size_aligned;

    if (kernel_header->SizeVisaDbgInBytes > 0) {
      std::stringstream ss;
      ss << dir_name << kernel_name << ".gpubin";

      size_t kernel_size = kernel_header->SizeVisaDbgInBytes;
      char *kernel_buffer = (char *)malloc(kernel_size);
      memcpy(kernel_buffer, ptr, kernel_size);

      auto elf_file = new ElfFile;
      if (elf_file->open(kernel_buffer, kernel_size, ss.str())) {
        // TODO(Keren): Dump binaries or not?
        FILE *fptr = fopen(ss.str().c_str(), "wb");
        fwrite(kernel_buffer, sizeof(char), kernel_size, fptr);
        fclose(fptr);

        elf_file->setIntelGPUFile(true);
        filevector->push_back(elf_file);
      } else {
        // kernel_buffer is released with elf_file
        delete elf_file;
      }
    } else {
      // Kernel does not have debug info
      return false;
    }

    ptr += kernel_header->SizeVisaDbgInBytes;
    ptr += kernel_header->SizeGenIsaDbgInBytes;
  }

  return true;
}
