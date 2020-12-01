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

// #include <lib/support/diagnostics.h>

#include "ElfHelper.hpp"
#include "RelocateCubin.hpp"

#include <Elf_X.h> // ensure EM_CUDA defined

#include <include/hpctoolkit-config.h>


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

  bool result = true;
#ifdef EM_CUDA
  if (ehdr->e_machine == EM_CUDA) {
    this->arch = ehdr->e_flags & 0xFF;
#ifdef DYNINST_USE_CUDA
    origPtr = (char *) malloc(memLen);
    memcpy(origPtr, memPtr, memLen);
    relocateCubin(memPtr, memLen, elf);
#else
    result = false;
    memPtr = 0;
#endif
  }
#endif

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
