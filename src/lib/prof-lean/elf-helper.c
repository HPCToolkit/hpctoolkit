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


      