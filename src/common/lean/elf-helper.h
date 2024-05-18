// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

//***************************************************************************
//
// File: elf-helper.h
//
// Purpose:
//   interface to query ELF binary information and hide the details about
//   extended number
//
//***************************************************************************

#ifndef ELF_HELPER_H
#define ELF_HELPER_H

#ifdef __cplusplus
extern "C" {
#endif


typedef struct elf_helper {
  Elf * elf;
  size_t section_string_index;
  Elf_Scn* symtab_section;
  Elf_Data *symtab_data;
  Elf_Scn* symtab_shndx_section;
  Elf_Data *symtab_shndx_data;
} elf_helper_t;

void
elf_helper_initialize
(
  Elf *elf,
  elf_helper_t* eh
);

GElf_Sym*
elf_helper_get_symbol
(
  elf_helper_t* eh,
  int index,
  GElf_Sym* sym_ptr,
  int* section_index_ptr
);

#ifdef __cplusplus
};
#endif


#endif
