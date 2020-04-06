// * BeginRiceCopyright *****************************************************
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
// File: cubin.c
//
// Purpose:
//   determine cubin symbol relocation values that will be used by hpcstruct
//
//***************************************************************************

//******************************************************************************
// system includes
//******************************************************************************

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gelf.h>

//******************************************************************************
// local includes
//******************************************************************************

#include <hpcrun/messages/messages.h>

#include "cubin-symbols.h"

//******************************************************************************
// macros
//******************************************************************************

#ifndef NULL
#define NULL 0
#endif

#define section_index(n) (n-1)

#define EM_CUDA 190

//******************************************************************************
// type definitions
//******************************************************************************

typedef struct Elf_SectionVector {
   int nsections;
   Elf_Scn **sections;
} Elf_SectionVector;

//******************************************************************************
// private functions
//******************************************************************************

static int
countSections
(
 Elf *elf
)
{
  int count = 0;
  Elf_Scn *scn = NULL;
  while ((scn = elf_nextscn(elf, scn)) != NULL) {
    count++;
  }
  return count;
}


static Elf_SectionVector *
newSectionVector(int nsections)
{
  Elf_SectionVector *v = (Elf_SectionVector *) malloc(sizeof(Elf_SectionVector));
  v->nsections = nsections;
  v->sections = (Elf_Scn **) calloc(nsections, sizeof(Elf_Scn *));
  return v;
}


// assemble a vector of pointers to each section in an elf binary
static Elf_SectionVector *
elfGetSectionVector
(
 Elf *elf
)
{
  int nsections = countSections(elf);
  if (nsections > 0) {
	  Elf_SectionVector *v = newSectionVector(nsections);
	  if (elf) {
		  Elf_Scn *scn = NULL;
		  int i=0;
		  while ((scn = elf_nextscn(elf, scn)) != NULL) {
			  v->sections[i++] = scn;
		  }
	  }
	  return v;
  }
  return NULL;
}


static size_t
sectionOffset
(
 Elf_SectionVector *sections,
 unsigned sindex
)
{
  // if section index is out of range, return 0
  if (sections->nsections < sindex) return 0;

  GElf_Shdr shdr;
  if (!gelf_getshdr(sections->sections[sindex], &shdr)) return 0;

  return shdr.sh_offset;
}


Elf_SymbolVector *
newSymbolsVector(int nsymbols)
{
  Elf_SymbolVector *v = (Elf_SymbolVector *) malloc(sizeof(Elf_SymbolVector));
  v->nsymbols = nsymbols;
  v->symbols = (unsigned long *) calloc(nsymbols, sizeof(unsigned long));
  return v;
}


static Elf_SymbolVector *
relocateSymbolsHelper
(
 Elf *elf,
 GElf_Ehdr *ehdr,
 GElf_Shdr *shdr,
 Elf_SectionVector *sections,
 Elf_Scn *scn
)
{
  Elf_SymbolVector *symbol_values = NULL;
  int nsymbols;
  assert (shdr->sh_type == SHT_SYMTAB);
  if (shdr->sh_entsize > 0) { // avoid divide by 0
    nsymbols = shdr->sh_size / shdr->sh_entsize;
    if (nsymbols <= 0) return NULL;
  } else {
    return NULL;
  }
  Elf_Data *datap = elf_getdata(scn, NULL);
  if (datap) {
    symbol_values = newSymbolsVector(nsymbols);
    for (int i = 0; i < nsymbols; i++) {
      GElf_Sym sym;
      GElf_Sym *symp = gelf_getsym(datap, i, &sym);
      if (symp) { // symbol properly read
	      int symtype = GELF_ST_TYPE(sym.st_info);
	      if (sym.st_shndx == SHN_UNDEF) continue;
	      switch(symtype) {
	        case STT_FUNC:
	          {
	            symbol_values->symbols[i] = sectionOffset(sections, section_index(sym.st_shndx));
	          }
	        default: break;
	      }
      }
    }
  }
  return symbol_values;
}


static Elf_SymbolVector *
relocateSymbols
(
  Elf *elf,
  Elf_SectionVector *sections
)
{
  Elf_SymbolVector *symbol_values = NULL;
  GElf_Ehdr ehdr_v;
  GElf_Ehdr *ehdr = gelf_getehdr(elf, &ehdr_v);
  if (ehdr) {
    for (int i = 0; i < sections->nsections; i++) {
      Elf_Scn *scn = sections->sections[i];
      GElf_Shdr shdr;
      if (!gelf_getshdr(scn, &shdr)) continue;
      if (shdr.sh_type == SHT_SYMTAB) {
	      symbol_values = relocateSymbolsHelper(elf, ehdr, &shdr, sections, scn);
	      break; // AFAIK, there can only be one symbol table
      }
    }
  }
  return symbol_values;
}


static Elf_SymbolVector *
computeSymbolOffsets
(
 char *cubin_ptr,
 Elf *cubin_elf
)
{
  Elf_SymbolVector *symbol_values = NULL;
  Elf_SectionVector *sections = elfGetSectionVector(cubin_elf);
  if (sections) {
    symbol_values = relocateSymbols(cubin_elf, sections);
    free(sections);
  }
  return symbol_values;
}


static void
__attribute__ ((unused))
printSymbols
(
  Elf_SymbolVector *symbols
)
{
  for (int i=0; i < symbols->nsymbols; i++) {
    TMSG(CUDA_CUBIN, "symbol %d: 0x%lx", i, symbols->symbols[i]);
  }
}

//******************************************************************************
// interface functions
//******************************************************************************

Elf_SymbolVector *
computeCubinFunctionOffsets
(
 const char *cubin_ptr_const,
 size_t cubin_len
)
{
  char *cubin_ptr = (char *) cubin_ptr_const;

  Elf_SymbolVector *symbols = NULL;
  elf_version(EV_CURRENT);
  Elf *elf = elf_memory(cubin_ptr, cubin_len);
  if (elf != 0) {
	  GElf_Ehdr ehdr_v;
	  GElf_Ehdr *ehdr = gelf_getehdr(elf, &ehdr_v);
	  if (ehdr) {
		  if (ehdr->e_machine == EM_CUDA) {
			  symbols = computeSymbolOffsets(cubin_ptr, elf);
			  if(ENABLED(CUDA_CUBIN))
				  printSymbols(symbols);
			  elf_end(elf);
		  }
	  }
  }

  return symbols;
}
