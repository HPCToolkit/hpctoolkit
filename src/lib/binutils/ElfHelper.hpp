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
//
// File: ElfHelper.hpp
//
// Purpose:
//   Interface for a module that scans an elf file and returns a vector
//   of sections 
//
//***************************************************************************

#ifndef __ElfHelper_hpp__
#define __ElfHelper_hpp__


//******************************************************************************
// system includes
//******************************************************************************

#include <vector>
#include <string>

#include <string.h>

#include <libelf.h>
#include <gelf.h>


//******************************************************************************
// macros
//******************************************************************************

#if 0
#undef DYNINST_USE_CUDA
#define DYNINST_USE_CUDA
#endif


//******************************************************************************
// type definitions
//******************************************************************************

class ElfFile {
public:
  ElfFile() : origPtr(0), memPtr(0), memLen(0), elf(0), intelGPU(false) {}
  bool open(char *_memPtr, size_t _memLen, const std::string &_fileName);
  ~ElfFile();
  int getArch() { return arch; }
  Elf *getElf() { return elf; }
  char *getMemory() { return memPtr; }
  char *getMemoryOriginal() { return origPtr; }
  size_t getLength() { return memLen; }
  std::string getFileName() { return fileName; }
  size_t getTextSection(char **text_section);
  bool isIntelGPUFile() { return intelGPU; }
  void setIntelGPUFile(bool _intelGPU) { intelGPU = _intelGPU; }
  // Intel GPUs have kernel name suffix
  void setGPUKernelName(const std::string &_gpuKernel) { gpuKernel = _gpuKernel; }
  std::string getGPUKernelName() { return gpuKernel; }
private:
  int arch;
  char *origPtr;
  char *memPtr;
  size_t memLen;
  Elf *elf;
  bool intelGPU;
  std::string fileName;
  std::string gpuKernel;
};

class ElfFileVector : public std::vector<ElfFile *> {};

class ElfSectionVector : public std::vector<Elf_Scn *> {};

//******************************************************************************
// interface functions
//******************************************************************************

ElfSectionVector *
elfGetSectionVector
(
 Elf *elf
);


char *
elfSectionGetData
(
 char *obj_ptr,
 GElf_Shdr *shdr
);


#endif
