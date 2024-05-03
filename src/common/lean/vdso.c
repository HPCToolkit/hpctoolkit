// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

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

  // free any resources allocated by elf_memory
  elf_end(vdso_elf);

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
