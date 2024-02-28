// * BeginRiceCopyright *****************************************************
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2024, Rice University
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
// File: zebinSymbols.c
//
// Purpose:
//   determine zebin symbol values so that gtpin can relocate
//   instruction-level measurements relative to the non-overlapping function
//   addresses in the binary
//
//***************************************************************************

//******************************************************************************
// system includes
//******************************************************************************

#define _GNU_SOURCE

#include <stdlib.h>
#include <string.h>

#include <gelf.h>



//******************************************************************************
// local includes
//******************************************************************************

#include "../../../../../../lib/prof-lean/elf-helper.h"

#include "zebinSymbols.h"



//******************************************************************************
// macros
//******************************************************************************

#undef  NULL
#define NULL 0

#undef  EM_INTELGT
#define EM_INTELGT 205  // Intel Graphics Technology

#define DEBUG_ZEBIN_SYMBOLS 0



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

  if (count > 0) {
    count++; // count section 0, which is skipped by elf_nextscn
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

      // placeholder for empty section 0 so that section number
      // indices into the vector will work as expected
      v->sections[i++] = 0;

      // collect the real sections
      while ((scn = elf_nextscn(elf, scn)) != NULL) {
        v->sections[i++] = scn;
      }
    }
    return v;
  }
  return NULL;
}


static const char *
getSymbolString(
  char *zebin_ptr,
  Elf *elf,
  Elf_SectionVector *sections,
  Elf64_Off symbolSectionOffset,
  Elf64_Word symbol_name_index
)
{
  const char *map_start = (const char *) zebin_ptr;
  const char *symbol_names = (const char *)(map_start + symbolSectionOffset);
  const char *symbol_name = symbol_names + symbol_name_index;
  return symbol_name;
}


static SymbolVector *
collectSymbolsHelper
(
 char *zebin_ptr,
 Elf *elf,
 GElf_Ehdr *ehdr,
 GElf_Shdr *shdr,
 Elf_SectionVector *sections,
 Elf_Scn *scn,
 elf_helper_t* eh
)
{
  SymbolVector *symbols = NULL;
  int nsymbols;
  if (shdr->sh_entsize > 0) { // avoid divide by 0
    nsymbols = shdr->sh_size / shdr->sh_entsize;
    if (nsymbols <= 0) return NULL;
  } else {
    return NULL;
  }

  // get header for symbol names section
  Elf_Scn *syms = sections->sections[shdr->sh_link];
  GElf_Shdr syms_shdr;
  if (!gelf_getshdr(syms, &syms_shdr)) return NULL;
  Elf64_Off symbolSectionOffset = syms_shdr.sh_offset;

  Elf_Data *datap = elf_getdata(scn, NULL);
  if (datap) {
    symbols = symbolVectorNew(nsymbols);
    for (int i = 0; i < nsymbols; i++) {
      GElf_Sym sym;
      GElf_Sym *symp = NULL;
      int section_index;
      symp = elf_helper_get_symbol(eh, i, &sym, &section_index);
      if (symp) { // symbol properly read
        int symtype = GELF_ST_TYPE(sym.st_info);
        if (sym.st_shndx == SHN_UNDEF) continue;
        switch(symtype) {
          case STT_FUNC:
            {
              const char *name = getSymbolString(zebin_ptr, elf, sections, symbolSectionOffset, sym.st_name);
              if (strcmp(name, "_entry") == 0) break; // ignore symbols named _entry
              symbolVectorAppend(symbols, name, sym.st_value);
              break;
            }
          default:
            break;
        }
      }
    }
  }
  return symbols;
}


static SymbolVector *
collectSymbols
(
  char *zebin_ptr,
  Elf *elf,
  elf_helper_t* eh,
   Elf_SectionVector *sections
)
{
  SymbolVector *symbols = NULL;
  GElf_Ehdr ehdr_v;
  GElf_Ehdr *ehdr = gelf_getehdr(elf, &ehdr_v);
  if (ehdr) {
    for (int i = 1; i < sections->nsections; i++) {
      Elf_Scn *scn = sections->sections[i];
      GElf_Shdr shdr;
      if (!gelf_getshdr(scn, &shdr)) continue;
      if (shdr.sh_type == SHT_SYMTAB) {
        symbols = collectSymbolsHelper(zebin_ptr, elf, ehdr, &shdr, sections, scn, eh);
        break; // AFAIK, there can only be one symbol table
      }
    }
  }
  return symbols;
}


static SymbolVector *
computeSymbolOffsets
(
 char *zebin_ptr,
 Elf *zebin_elf,
 elf_helper_t* eh
)
{
  SymbolVector *symbols = NULL;
  Elf_SectionVector *sections = elfGetSectionVector(zebin_elf);
  if (sections) {
    symbols = collectSymbols(zebin_ptr, zebin_elf, eh, sections);
    free(sections);
  }
  return symbols;
}



//******************************************************************************
// interface functions
//******************************************************************************

SymbolVector *
collectZebinSymbols
(
 const char *zebin_ptr_const,
 size_t zebin_len
)
{
  char *zebin_ptr = (char *) zebin_ptr_const;

  SymbolVector *symbols = NULL;
  elf_version(EV_CURRENT);
  Elf *elf = elf_memory(zebin_ptr, zebin_len);
  elf_helper_t eh;
  elf_helper_initialize(elf, &eh);
  if (elf != 0) {
    GElf_Ehdr ehdr_v;
    GElf_Ehdr *ehdr = gelf_getehdr(elf, &ehdr_v);
    if (ehdr) {
      if (ehdr->e_machine == EM_INTELGT) {
        symbols = computeSymbolOffsets(zebin_ptr, elf, &eh);
        if (DEBUG_ZEBIN_SYMBOLS) {
          symbolVectorPrint(symbols, "zeBinary Symbols");
        }
        elf_end(elf);
      }
    }
  }

  return symbols;
}
