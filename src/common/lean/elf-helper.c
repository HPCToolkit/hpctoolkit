// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

//***************************************************************************
//
// File: elf-helper.c
//
// Purpose:
//   interface implementation for querying ELF binary information and
//   hiding the details about extended number
//
//***************************************************************************


//******************************************************************************
// system includes
//******************************************************************************

#include <libelf.h>
#include <gelf.h>
#include <stddef.h>



//******************************************************************************
// local includes
//******************************************************************************

#include "elf-helper.h"

//******************************************************************************
// interface functions
//******************************************************************************


void
elf_helper_initialize
(
  Elf *elf,
  elf_helper_t* eh
)
{
  eh->elf = elf;
  elf_getshdrstrndx(elf, &(eh->section_string_index));
  eh->symtab_section = NULL;
  eh->symtab_data = NULL;
  eh->symtab_shndx_section = NULL;
  eh->symtab_shndx_data = NULL;

  // Find .symtab and .symtab_shndx
  Elf_Scn *scn = NULL;
  while ((scn = elf_nextscn(elf, scn)) != NULL) {
    GElf_Shdr shdr;
    if (!gelf_getshdr(scn, &shdr)) continue;
    if (shdr.sh_type == SHT_SYMTAB_SHNDX) {
      // If .symtab_shndx section exists, we need extended numbering
      // for section index of a symbol
      eh->symtab_shndx_section = scn;
      eh->symtab_shndx_data = elf_getdata(scn, NULL);
    }
    if (shdr.sh_type == SHT_SYMTAB) {
      eh->symtab_section = scn;
      eh->symtab_data = elf_getdata(scn, NULL);
    }
  }
}


GElf_Sym*
elf_helper_get_symbol
(
  elf_helper_t* eh,
  int index,
  GElf_Sym* sym_ptr,
  int* section_index_ptr
)
{
  GElf_Sym* symp;
  Elf32_Word xndx;
  if (eh->symtab_shndx_data != NULL) {
    symp = gelf_getsymshndx(eh->symtab_data, eh->symtab_shndx_data, index, sym_ptr, &xndx);
  } else {
    symp = gelf_getsym(eh->symtab_data, index, sym_ptr);
  }

  // Handle extended numbering if needed
  // and store the symbol section index
  *section_index_ptr = symp->st_shndx;
  if (*section_index_ptr == SHN_XINDEX) {
    *section_index_ptr = xndx;
  }
  return symp;
}
