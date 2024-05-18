// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

//***************************************************************************
//
// File: ElfHelper.cpp
//
// Purpose:
//   implementation of a function that scans an elf file and returns a vector
//   of sections
//
//***************************************************************************


//******************************************************************************
// system includes
//******************************************************************************

#include <err.h>
#include <gelf.h>



//******************************************************************************
// local includes
//******************************************************************************

// #include "../common/diagnostics.h"

#include "ElfHelper.hpp"
#include "RelocateCubin.hpp"
#include "Fatbin.hpp"

#include <Elf_X.h> // ensure EM_CUDA defined


//******************************************************************************
// macros
//******************************************************************************

#ifndef NULL
#define NULL 0
#endif



//******************************************************************************
// interface functions
//******************************************************************************

bool
ElfFile::open
(
 char *_memPtr,
 size_t _memLen,
 const std::string &_fileName
)
{
  origPtr = _memPtr;
  memPtr = _memPtr;
  memLen = _memLen;
  fileName = _fileName;

  elf_version(EV_CURRENT);
  elf = elf_memory(memPtr, memLen);
  if (elf == 0 || elf_kind(elf) != ELF_K_ELF) {
    memPtr = 0;
    return false;
  }

  GElf_Ehdr ehdr_v;
  GElf_Ehdr *ehdr = gelf_getehdr(elf, &ehdr_v);
  if (!ehdr) {
    memPtr = 0;
    return false;
  }

  this->arch = ehdr->e_flags & 0xFF;

  bool result = true;

#ifdef EM_CUDA
  if (ehdr->e_machine == EM_CUDA) {
#ifdef DYNINST_USE_CUDA
    origPtr = (char *) malloc(memLen);
    memcpy(origPtr, memPtr, memLen);
    relocateCubin(memPtr, memLen, elf);

    if (getenv("HPCSTRUCT_CUBIN_RELOCATION")) {
      std::string newname = fileName + ".relocated";
      FILE *f = fopen(newname.c_str(), "w");
      fwrite(getMemory(), getLength(), 1, f);
      fclose(f);
    }
#else
    result = false;
    memPtr = 0;
#endif
  }
#endif

  switch(ehdr->e_machine) {
#ifdef EM_INTEL_GEN9
  case EM_INTEL_GEN9:
    intelGPU = true;
    break;
#endif
#ifdef EM_INTELGT
  case EM_INTELGT:
    intelGPU = true;
    break;
#endif
  default:
    intelGPU = false;
    break;
  }

  return result;
}


ElfFile::~ElfFile()
{
  if (origPtr != memPtr && origPtr != 0) free(origPtr);
  elf_end(elf);
}


// assemble a vector of pointers to each section in an elf binary
ElfSectionVector *
elfGetSectionVector
(
 Elf *elf
)
{
  ElfSectionVector *sections = new ElfSectionVector;
  bool nonempty = false;
  if (elf) {
    Elf_Scn *scn = NULL;
    while ((scn = elf_nextscn(elf, scn)) != NULL) {
      sections->push_back(scn);
      nonempty = true;
    }
  }
  if (nonempty) return sections;
  else {
    delete sections;
    return NULL;
  }
}


char *
elfSectionGetData
(
 char *obj_ptr,
 GElf_Shdr *shdr
)
{
  char *sectionData = obj_ptr + shdr->sh_offset;
  return sectionData;
}


size_t
ElfFile::getTextSection
(
 char **text_section
)
{
  // start cfg generation
  ElfSectionVector *sections = elfGetSectionVector(elf);
  GElf_Ehdr ehdr_v;
  GElf_Ehdr *ehdr = gelf_getehdr(elf, &ehdr_v);

  if (ehdr) {
    for (auto si = sections->begin(); si != sections->end(); si++) {
      Elf_Scn *scn = *si;
      GElf_Shdr shdr_v;
      GElf_Shdr *shdr = gelf_getshdr(scn, &shdr_v);
      if (!shdr) continue;
      char *sectionData = elfSectionGetData(memPtr, shdr);
      const char *section_name = elf_strptr(elf, ehdr->e_shstrndx, shdr->sh_name);
      if (strcmp(section_name, ".text") == 0) {
        // TODO(Aaron): can a intel GPU binary has two text sections?
        *text_section = sectionData;
        return shdr->sh_size;
      }
    }
  }

  return 0;
}
