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
// File:
//   vdso.c
//
// Purpose:
//   interface for information about VDSO segment in linux
//
// Description:
//   identify VDSO segment and its properties
//
//***************************************************************************

//***************************************************************************
// system includes
//***************************************************************************

#include <string.h>
#include <sys/auxv.h>  // getauxval

#include <libelf.h>
#include <gelf.h>

//***************************************************************************
// local includes
//***************************************************************************

#include "vdso.h"
#include "procmaps.h"



//***************************************************************************
// type declarations
//***************************************************************************

typedef struct {
  void *addr;
  size_t size;
} vdso_info_t;


// Each process running a node has one vdso
static char vdso_saved_path[PATH_MAX];
static size_t vdso_length;

//***************************************************************************
// interface operations
//***************************************************************************

int
vdso_segment_p
(
 const char *filename
)
{
  int result = 0;
  result |= (strcmp(filename, VDSO_SEGMENT_NAME_SHORT) == 0);
  result |= (strcmp(filename, VDSO_SEGMENT_NAME_LONG) == 0);
  return result;
}


void *
vdso_segment_addr
(
 void
)
{
  return (void*) getauxval(AT_SYSINFO_EHDR);
}


size_t
vdso_segment_len
(
 void
)
{
  if (vdso_length > 0) {
    return vdso_length;
  }
  void *vdso_addr = vdso_segment_addr();

  size_t seg_len = 0;
  if (vdso_addr) {
    lm_seg_t *s = lm_segment_find_by_name(VDSO_SEGMENT_NAME_SHORT);
    if (s) seg_len = lm_segment_length(s);
  }

  size_t elf_size = 0;
  elf_version(EV_CURRENT);
  Elf *vdso_elf = elf_memory(vdso_addr, seg_len); 
  GElf_Ehdr ehdr;
  if (vdso_elf && gelf_getehdr(vdso_elf, &ehdr)) {
    // ELF Format:
    // | ELF HEADER | Program Header |  Section 1 | ... | Section n | Section Header |
    // So, to get the size of vdso, we need to to get the end of section header.
    // The section header is a table, each entry describing a section
    size_t section_count = 0;
    if (elf_getshdrnum(vdso_elf, &section_count) == 0) {
      elf_size = ehdr.e_shoff + ehdr.e_shentsize * section_count;
    }
  }
  if (elf_size > 0)
    return vdso_length = elf_size;
  else
    return vdso_length = seg_len;
}

const char*
get_saved_vdso_path()
{
  return vdso_saved_path;
}

int
set_saved_vdso_path(const char* path)
{
  if (strlen(path) >= PATH_MAX) return 1;
  strcpy(vdso_saved_path, path);
  return 0;
}


