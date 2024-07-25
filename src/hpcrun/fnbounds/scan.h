// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

// scan.h - program to print the function list from its load-object arguments

#ifndef _FNBOUNDS_SCAN_H_
#define _FNBOUNDS_SCAN_H_

#include "fnbounds.h"
#include <libelf.h>
#include <gelf.h>
#include <stdbool.h>

typedef struct __ehRecord {
  Elf_Scn   *ehHdrSection;
  Elf_Scn   *ehFrameSection;
  Elf_Scn   *textSection;
  Elf_Scn   *dataSection;
  size_t    ehHdrIndex;
  size_t    ehFrameIndex;
} ehRecord_t;

// prototypes
bool fnb_pltscan(FnboundsResponse*, Elf *e, GElf_Shdr sh, const char* xname);
bool fnb_pltsecscan(FnboundsResponse*, Elf *e, GElf_Shdr sh, const char* xname);
bool fnb_initscan(FnboundsResponse*, Elf *e, GElf_Shdr sh, const char* xname);
bool fnb_textscan(FnboundsResponse*, Elf *e, GElf_Shdr sh, const char* xname);
bool fnb_finiscan(FnboundsResponse*, Elf *e, GElf_Shdr sh, const char* xname);
bool fnb_altinstr_replacementscan(FnboundsResponse*, Elf *e, GElf_Shdr sh, const char* xname);
bool fnb_ehframescan(FnboundsResponse*, Elf *e, ehRecord_t *ehRecord, const char* xname);
bool fnb_doSectionScan(Elf *e, GElf_Shdr secHead, const char* xname);

#endif  // _FNBOUNDS_SCAN_H_
