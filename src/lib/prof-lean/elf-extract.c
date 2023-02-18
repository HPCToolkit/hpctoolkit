// * BeginRiceCopyright *****************************************************
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2023, Rice University
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
