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
// Copyright ((c)) 2002-2021, Rice University
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

//******************************************************************************
// system includes
//******************************************************************************

#ifndef BANAL_GPU_READ_INTEL_CFG_HPP
#define BANAL_GPU_READ_INTEL_CFG_HPP

#include <string>
#include <Symtab.h>
#include <CodeSource.h>
#include <CodeObject.h>

#include <lib/binutils/ElfHelper.hpp>
#include "DotCFG.hpp"



//******************************************************************************
// type definitions
//*****************************************************************************

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
readIntelCFG
(
 const std::string &search_path,
 ElfFile *elfFile,
 Dyninst::SymtabAPI::Symtab *the_symtab, 
 bool cfg_wanted,
 bool du_graph_wanted,
 Dyninst::ParseAPI::CodeSource **code_src, 
 Dyninst::ParseAPI::CodeObject **code_obj
);

#endif
