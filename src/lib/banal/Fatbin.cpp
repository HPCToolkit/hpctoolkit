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
// Copyright ((c)) 2002-2018, Rice University
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
// File: Fatbin.cpp
//
// Purpose:
//   unpack cuda fatbin 
//
// Description:
//
//***************************************************************************

//******************************************************************************
// system includes
//******************************************************************************

#include <stdio.h>
#include <string.h>



//******************************************************************************
// local includes
//******************************************************************************

#include <lib/support/StrUtil.hpp>
#include <include/hpctoolkit-config.h>

#include <Elf_X.h>

#include "InputFile.hpp"
#include "ElfHelper.hpp"



//******************************************************************************
// macros
//******************************************************************************


#define CUDA_FATBIN_SECTION  	".nvFatBinSegment"
#define CUDA_FATBIN_DATASECTION ".nv_fatbin"

#define CUDA_FATBIN_MAGIC 	0x466243b1
#define CUDA_FATBIN_VERSION 	1



//******************************************************************************
// type definitions
//******************************************************************************

typedef struct NvidiaFatBinHeader_s {
  int32_t magicNumber; 
  int32_t version; 
  const unsigned long long* data; 
  void* unused;
} NvidiaFatBinHeader_t; 


typedef std::vector<NvidiaFatBinHeader_t *> FatbinSectionVector;



//******************************************************************************
// local variables
//******************************************************************************

static FatbinSectionVector fatbinSectionVector;



//******************************************************************************
// private functions
//******************************************************************************

#ifdef DYNINST_USE_CUDA

static bool
isCubin(Elf *elf)
{
  // open the header of the Elf object
  GElf_Ehdr ehdr_v; 
  GElf_Ehdr *obj_ehdr = gelf_getehdr(elf, &ehdr_v);

  // check the header of the Elf object to see if it is a Cubin
  return (obj_ehdr && (obj_ehdr->e_machine == EM_CUDA));
}


static bool
recordIfNvFatbin
(
 char *obj_ptr,
 Elf *elf,
 GElf_Ehdr *ehdr,
 Elf_Scn *scn,
 GElf_Shdr *shdr
 )
{
#if 0
  bool isFatbin = strcmp(elf_strptr(elf, ehdr->e_shstrndx, shdr->sh_name),
			 CUDA_FATBIN_DATASECTION) == 0;
  if (isFatbin) { 
    NvidiaFatBinHeader_t *fatbin =
      (NvidiaFatBinHeader_t *) elfSectionGetData(obj_ptr, shdr);
    std::string empty;
    ElfFile *elfFile = new ElfFile;
    if (elfFile->open((char *) fatbin->data, (size_t) shdr->sh_size, empty)) {
      fatbinSectionVector.push_back(fatbin);
    }
  }
  return isFatbin;
#else  
  return false;
#endif
}


static bool
recordIfCubin
(
 ElfFile *loadModule,
 char *obj_ptr,
 Elf *elf,
 GElf_Ehdr *ehdr,
 Elf_Scn *scn,
 GElf_Shdr *shdr,
 ElfFileVector *elfFileVector
 )
{
  char *sectionData = elfSectionGetData(obj_ptr, shdr);

  std::string filename = loadModule->getFileName() + "@" +
    StrUtil::toStr(shdr->sh_addr, 16);

  ElfFile *elfFile = new ElfFile;

  // check if section represents an Elf object
  if (elfFile->open(sectionData, shdr->sh_size, filename)) {

    // if the Elf file is a CUBIN, add it to the vector of load
    // modules to be analyzed
    if (isCubin(elfFile->getElf())) {
      elfFileVector->push_back(elfFile);
      return true;
    } else {
      delete elfFile;
    }
  }
  return false;
}
#endif
	

// cubin text segments all start at offset 0 and are thus overlapping.
// relocate each text segment so that it begins at its offset in the
// cubin. when this function returns, text segments no longer overlap.
static bool
findCubinSections
(
 ElfFile *elfFile,
 char *obj_ptr,
 Elf *elf,
 ElfSectionVector *sections,
 ElfFileVector *elfFileVector
 )
{
  int count = 0;

#ifdef DYNINST_USE_CUDA
  GElf_Ehdr ehdr_v;
  GElf_Ehdr *ehdr = gelf_getehdr(elf, &ehdr_v);

  if (ehdr) {
    for (auto si = sections->begin(); si != sections->end(); si++) {
      Elf_Scn *scn = *si;
      GElf_Shdr shdr_v;
      GElf_Shdr *shdr = gelf_getshdr(scn, &shdr_v);
      if (!shdr) continue;

      if (recordIfNvFatbin(obj_ptr, elf, ehdr, scn, shdr)) {
	count++;
      } else if (recordIfCubin(elfFile, obj_ptr, elf, ehdr, scn,
			       shdr, elfFileVector)) {
	count++;
      }
    }
  }
#endif
  return count > 0;
}



//******************************************************************************
// interface functions
//******************************************************************************

bool
findCubins
(
 ElfFile *elfFile,
 ElfFileVector *elfFileVector
)
{
  bool success = false;
  Elf *elf = elfFile->getElf();

  ElfSectionVector *sections = elfGetSectionVector(elf);
  
  if (sections) {
    success = findCubinSections(elfFile, elfFile->getMemory(), elf,
			     sections, elfFileVector);
  }
  return success;
}



//******************************************************************************
// debugging support
//******************************************************************************

void
writeElfFile
(
 ElfFile *elfFile,
 const char *suffix
)
{
  std::string filename = elfFile->getFileName() + suffix;
  FILE *f = fopen(filename.c_str(), "w");
  fwrite(elfFile->getMemory(), elfFile->getLength(), 1, f);
  fclose(f);
}


#ifdef DYNINST_USE_CUDA
void
writeCubins(
 ElfFileVector *elfFileVector
)
{
  for(unsigned int i = 0; i < elfFileVector->size(); i++) {
     ElfFile *elfFile = (*elfFileVector)[i];
     if (isCubin(elfFile->getElf())) {
       writeElfFile(elfFile, ".cubin");
     }
  }
}
#endif
