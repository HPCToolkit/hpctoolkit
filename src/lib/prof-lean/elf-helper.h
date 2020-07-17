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