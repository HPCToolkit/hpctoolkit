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
// File: RelocateCubin.cpp
//
// Purpose:
//   Implementation of in-memory cubin relocation.
//
// Description:
//   The associated implementation performs in-memory relocation of a cubin so
//   that all text segments and functions are non-overlapping. Following
//   relocation
//     - each text segment begins at its offset in the segment
//     - each function, which is in a unique text segment, has its symbol
//       adjusted to point to the new position of the function in its relocated
//       text segment
//     - addresses in line map entries are adjusted to account for the new
//       offsets of the instructions to which they refer
//
//***************************************************************************

//******************************************************************************
// system includes
//******************************************************************************

#include <assert.h>
#include <string.h>



//******************************************************************************
// local includes
//******************************************************************************

#include "ElfHelper.hpp"
#include "RelocateCubin.hpp"



//******************************************************************************
// macros
//******************************************************************************

#define DEBUG_LINE_SECTION_NAME ".debug_line"

#define DEBUG_INFO_SECTION_NAME ".debug_info"

#define CASESTR(n) case n: return #n
#define section_index(n) (n-1)

#define DEBUG_CUBIN_RELOCATION 0


//---------------------------------------------------------
// NVIDIA CUDA line map relocation types 
//   type name gleaned using cuobjdump
//   value gleaned by examining binaries 
//---------------------------------------------------------
#define R_NV_32                 0x01
#define R_NV_64                 0x02
#define R_NV_G32                0x03
#define R_NV_G64                0x04

#define RELOC_32(x) (x == R_NV_32 || x == R_NV_G32)
#define RELOC_64(x) (x == R_NV_64 || x == R_NV_G64)



//******************************************************************************
// type definitions
//******************************************************************************

typedef std::vector<Elf_Scn *> Elf_SectionVector;
typedef std::vector<Elf64_Addr> Elf_SymbolVector;



//******************************************************************************
// private functions
//******************************************************************************

static size_t
sectionOffset
(
 Elf_SectionVector *sections,
 unsigned sindex
)
{
  // if section index is out of range, return 0
  if (sections->size() < sindex) return 0;

  GElf_Shdr shdr;
  if (!gelf_getshdr((*sections)[sindex], &shdr)) return 0;

  return shdr.sh_offset;
}


#if DEBUG_CUBIN_RELOCATION
static const char *
binding_name
(
 GElf_Sym *s
)
{
  switch(GELF_ST_BIND(s->st_info)){
    CASESTR(STB_LOCAL);
    CASESTR(STB_GLOBAL);
    CASESTR(STB_WEAK);
  default: return "UNKNOWN";
  }
}
#endif


// properly size the relocation update to an address based on the
// relocation type
static void
applyRelocation(void *addr, unsigned rel_type, uint64_t rel_value)
{
  if (RELOC_64(rel_type)) {
    uint64_t *addr64 = (uint64_t *) addr;
    *addr64 = rel_value;
  } else if (RELOC_32(rel_type)) {
    uint32_t *addr32 = (uint32_t *) addr;
    *addr32 = rel_value;
  } else {
    assert(0);
  }
}


static void
applyRELrelocation
(
 char *line_map,
 Elf_SymbolVector *symbol_values,
 GElf_Rel *rel
)
{
  // get the symbol that is the basis for the relocation
  unsigned sym_index = GELF_R_SYM(rel->r_info);

  // determine the type of relocation
  unsigned rel_type = GELF_R_TYPE(rel->r_info);

  // get the new offset of the aforementioned symbol
  unsigned sym_value = (*symbol_values)[sym_index];

  // compute address where a relocation needs to be applied
  void *addr = (void *) (line_map + rel->r_offset);

  // update the address based on the symbol value associated with the
  // relocation entry.
  applyRelocation(addr, rel_type, sym_value);
}


static void
applyRELArelocation
(
 char *debug_info,
 Elf_SymbolVector *symbol_values,
 GElf_Rela *rela
)
{
  // get the symbol that is the basis for the relocation
  unsigned sym_index = GELF_R_SYM(rela->r_info);

  // determine the type of relocation
  unsigned rel_type = GELF_R_TYPE(rela->r_info);

  // get the new offset of the aforementioned symbol
  unsigned sym_value = (*symbol_values)[sym_index];

  // compute the address where a relocation needs to be applied
  void *addr = (unsigned long *) (debug_info + rela->r_offset);

  // update the address in based on the symbol value and addend in the
  // relocation entry
  applyRelocation(addr, rel_type, sym_value + rela->r_addend);
}


static void
applyLineMapRelocations
(
 Elf_SymbolVector *symbol_values,
 char *line_map,
 int n_relocations,
 Elf_Data *relocations_data
)
{
  //----------------------------------------------------------------
  // apply each line map relocation
  //----------------------------------------------------------------
  for (int i = 0; i < n_relocations; i++) {
    GElf_Rel rel_v;
    GElf_Rel *rel = gelf_getrel(relocations_data, i, &rel_v);
    applyRELrelocation(line_map, symbol_values, rel);
  }
}


// if the cubin contains a line map section and a matching line map relocations
// section, apply the relocations to the line map
static void
relocateLineMap
(
 char *cubin_ptr,
 Elf *elf,
 Elf_SectionVector *sections,
 Elf_SymbolVector *symbol_values
)
{
  GElf_Ehdr ehdr_v;
  GElf_Ehdr *ehdr = gelf_getehdr(elf, &ehdr_v);
  if (ehdr) {
    unsigned line_map_scn_index;
    char *line_map = NULL;

    //-------------------------------------------------------------
    // scan through the sections to locate a line map, if any
    //-------------------------------------------------------------
    int index = 0;
    for (auto si = sections->begin(); si != sections->end(); si++, index++) {
      Elf_Scn *scn = *si;
      GElf_Shdr shdr;
      if (!gelf_getshdr(scn, &shdr)) continue;
      if (shdr.sh_type == SHT_PROGBITS) {
	const char *section_name = elf_strptr(elf, ehdr->e_shstrndx, shdr.sh_name);
	if (strcmp(section_name, DEBUG_LINE_SECTION_NAME) == 0) {
	  // remember the index of line map section. we need this index to find
	  // the corresponding relocation section.
	  line_map_scn_index = index;

	  // compute line map position from start of cubin and the offset
	  // of the line map section in the cubin
	  line_map = cubin_ptr + shdr.sh_offset;

	  // found the line map, so we are done with the linear scan of sections
	  break;
	}
      }
    }

    //-------------------------------------------------------------
    // if a line map was found, look for its relocations section
    //-------------------------------------------------------------
    if (line_map) {
      int index = 0;
      for (auto si = sections->begin(); si != sections->end(); si++, index++) {
	Elf_Scn *scn = *si;
	GElf_Shdr shdr;
	if (!gelf_getshdr(scn, &shdr)) continue;
	if (shdr.sh_type == SHT_REL)  {
	  if (section_index(shdr.sh_info) == line_map_scn_index) {
	    // the relocation section refers to the line map section
#if DEBUG_CUBIN_RELOCATION
	    std::cout << "line map relocations section name: "
		      << elf_strptr(elf, ehdr->e_shstrndx, shdr.sh_name)
		      << std::endl;
#endif
	    //-----------------------------------------------------
	    // invariant: have a line map and its relocations.
	    // use new symbol values to relocate the line map entries.
	    //-----------------------------------------------------

	    // if the relocation entry size is not positive, give up
	    if (shdr.sh_entsize > 0) {
	      int n_relocations = shdr.sh_size / shdr.sh_entsize;
	      if (n_relocations > 0) {
		Elf_Data *relocations_data = elf_getdata(scn, NULL);
		applyLineMapRelocations(symbol_values, line_map,
					   n_relocations, relocations_data);
	      }
	    }
	    return;
	  }
	}
      }
    }
  }
}


static void
applyDebugInfoRelocations
(
 Elf_SymbolVector *symbol_values,
 char *debug_info,
 int n_relocations,
 Elf_Data *relocations_data
)
{
  //----------------------------------------------------------------
  // apply each debug info relocation
  //----------------------------------------------------------------
  for (int i = 0; i < n_relocations; i++) {
    GElf_Rela rela_v;
    GElf_Rela *rela = gelf_getrela(relocations_data, i, &rela_v);
    applyRELArelocation(debug_info, symbol_values, rela);
  }
}


// if the cubin contains a line map section and a matching line map relocations
// section, apply the relocations to the line map
static void
relocateDebugInfo
(
 char *cubin_ptr,
 Elf *elf,
 Elf_SectionVector *sections,
 Elf_SymbolVector *symbol_values
)
{
  GElf_Ehdr ehdr_v;
  GElf_Ehdr *ehdr = gelf_getehdr(elf, &ehdr_v);
  if (ehdr) {
    unsigned debug_info_scn_index;
    char *debug_info = NULL;

    //-------------------------------------------------------------
    // scan through the sections to locate debug info, if any
    //-------------------------------------------------------------
    int index = 0;
    for (auto si = sections->begin(); si != sections->end(); si++, index++) {
      Elf_Scn *scn = *si;
      GElf_Shdr shdr;
      if (!gelf_getshdr(scn, &shdr)) continue;
      if (shdr.sh_type == SHT_PROGBITS) {
	const char *section_name = elf_strptr(elf, ehdr->e_shstrndx, shdr.sh_name);
	if (strcmp(section_name, DEBUG_INFO_SECTION_NAME) == 0) {
	  // remember the index of line map section. we need this index to find
	  // the corresponding relocation section.
	  debug_info_scn_index = index;

	  // compute debug info position from start of cubin and the offset
	  // of the debug info section in the cubin
	  debug_info = cubin_ptr + shdr.sh_offset;

	  // found the debug info, so we are done with the linear scan of sections
	  break;
	}
      }
    }

    //-------------------------------------------------------------
    // if debug info was found, look for its relocations section
    //-------------------------------------------------------------
    if (debug_info) {
      int index = 0;
      for (auto si = sections->begin(); si != sections->end(); si++, index++) {
	Elf_Scn *scn = *si;
	GElf_Shdr shdr;
	if (!gelf_getshdr(scn, &shdr)) continue;
	if (shdr.sh_type == SHT_RELA)  {
	  if (section_index(shdr.sh_info) == debug_info_scn_index) {
	    // the relocation section refers to the line map section
#if DEBUG_CUBIN_RELOCATION
	    std::cout << "debug info relocations section name: "
		      << elf_strptr(elf, ehdr->e_shstrndx, shdr.sh_name)
		      << std::endl;
#endif
	    //-----------------------------------------------------
	    // invariant: have debug info and its relocations.
	    // use new symbol values to relocate debug info entries.
	    //-----------------------------------------------------

	    // if the relocation entry size is not positive, give up
	    if (shdr.sh_entsize > 0) {
	      int n_relocations = shdr.sh_size / shdr.sh_entsize;
	      if (n_relocations > 0) {
		Elf_Data *relocations_data = elf_getdata(scn, NULL);
		applyDebugInfoRelocations(symbol_values, debug_info,
					   n_relocations, relocations_data);
	      }
	    }
	    return;
	  }
	}
      }
    }
  }
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
  int nsymbols = 0;
  assert (shdr->sh_type == SHT_SYMTAB);
  if (shdr->sh_entsize > 0) { // avoid divide by 0
    nsymbols = shdr->sh_size / shdr->sh_entsize;
  }
  if (nsymbols <= 0) return NULL;

  Elf_Data *datap = elf_getdata(scn, NULL);
  if (datap) {
    symbol_values = new Elf_SymbolVector(nsymbols);
    for (int i = 0; i < nsymbols; i++) {
      GElf_Sym sym;
      GElf_Sym *symp = gelf_getsym(datap, i, &sym);
      if (symp) { // symbol properly read
	int symtype = GELF_ST_TYPE(sym.st_info);
	if (sym.st_shndx == SHN_UNDEF) continue;
	switch(symtype) {
	case STT_FUNC:
	  {
	    int64_t s_offset = sectionOffset(sections, section_index(sym.st_shndx));
#if DEBUG_CUBIN_RELOCATION
	    Elf64_Addr addr_signed = sym.st_value;
	    std::cout << "elf symbol " << elf_strptr(elf, shdr->sh_link, sym.st_name)
		      << " value=0x" << std::hex << addr_signed
		      << " binding=" << binding_name(&sym)
		      << " section=" << std::dec << section_index(sym.st_shndx)
		      << " section offset=0x" << std::hex << s_offset
		      << std::endl;
#endif
	    // update each function symbol's offset to match the new offset of the
	    // text section that contains it.
	    sym.st_value = (Elf64_Addr) s_offset;
	    gelf_update_sym(datap, i, &sym);
	    (*symbol_values)[i] = s_offset;
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
    for (auto si = sections->begin(); si != sections->end(); si++) {
      Elf_Scn *scn = *si;
      GElf_Shdr shdr;
      if (!gelf_getshdr(scn, &shdr)) continue;
      if (shdr.sh_type == SHT_SYMTAB) {
#if DEBUG_CUBIN_RELOCATION
        std::cout << "relocating symbols in section " << elf_strptr(elf, ehdr->e_shstrndx, shdr.sh_name)
		  << std::endl;
#endif
	symbol_values = relocateSymbolsHelper(elf, ehdr, &shdr, sections, scn);
	break; // AFAIK, there can only be one symbol table
      }
    }
  }
  return symbol_values;
}


// cubin text segments all start at offset 0 and are thus overlapping.
// relocate each segment of type SHT_PROGBITS (a program text
// or data segment) so that it begins at its offset in the
// cubin. when this function returns, text segments no longer overlap.
static void
relocateProgramDataSegments
(
  Elf *elf,
  Elf_SectionVector *sections
)
{
  GElf_Ehdr ehdr_v;
  GElf_Ehdr *ehdr = gelf_getehdr(elf, &ehdr_v);
  if (ehdr) {
    for (auto si = sections->begin(); si != sections->end(); si++) {
      Elf_Scn *scn = *si;
      GElf_Shdr shdr;
      if (!gelf_getshdr(scn, &shdr)) continue;
      if (shdr.sh_type == SHT_PROGBITS) {
#if DEBUG_CUBIN_RELOCATION
        std::cout << "relocating section " << elf_strptr(elf, ehdr->e_shstrndx, shdr.sh_name)
		  << std::endl;
#endif
	// update a segment so it's starting address matches its offset in the
	// cubin.
	shdr.sh_addr = shdr.sh_offset;
	gelf_update_shdr(scn, &shdr);
      }
    }
  }
}



//******************************************************************************
// interface functions
//******************************************************************************

bool
relocateCubin
(
 char *cubin_ptr,
 Elf *cubin_elf
)
{
  bool success = false;

  Elf_SectionVector *sections = elfGetSectionVector(cubin_elf);
  if (sections) {
    relocateProgramDataSegments(cubin_elf, sections);
    Elf_SymbolVector *symbol_values = relocateSymbols(cubin_elf, sections);
    if (symbol_values) {
      relocateLineMap(cubin_ptr, cubin_elf, sections, symbol_values);
      relocateDebugInfo(cubin_ptr, cubin_elf, sections, symbol_values);
      delete symbol_values;
      success = true;
    }
    delete sections;
  }

  return success;
}
