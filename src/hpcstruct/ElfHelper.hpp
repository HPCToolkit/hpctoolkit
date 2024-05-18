// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

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
