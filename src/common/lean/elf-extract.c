// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

//***************************************************************************
//
// File: elf-extract.c
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
#include <string.h>



//******************************************************************************
// local includes
//******************************************************************************

#include "elf-extract.h"

//******************************************************************************
// interface functions
//******************************************************************************


bool
elf_section_info
(
 unsigned char *binary,
 size_t binary_size,
 const char *section_name,
 unsigned char **section,
 size_t *section_size
)
{
  // sanity check
  elf_version(EV_CURRENT);

  // open elf file
  Elf *elf = elf_memory((char *) binary, binary_size);

  if (elf) {
    // locate pointer to string table containing section names
    size_t section_string_index;
    elf_getshdrstrndx(elf, &section_string_index);

    // find section by name
    Elf_Scn *scn = NULL;
    while ((scn = elf_nextscn(elf, scn)) != NULL) {
      // extract section header
      GElf_Shdr shdr;
      if (!gelf_getshdr(scn, &shdr)) continue;

      // extract section name from header
      const char *sec_name = elf_strptr(elf, section_string_index, shdr.sh_name);

      // return info if section has target name
      if (strcmp(section_name, sec_name) == 0) {
        *section = binary + shdr.sh_offset;
        *section_size = shdr.sh_size;
        return true;
      }
    }

    // free any resources allocated by elf_memory
    elf_end(elf);
  }

  return false;
}
