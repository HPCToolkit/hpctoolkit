// -*-Mode: C++;-*- // technically C99

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

#define _GNU_SOURCE
#include <link.h>
#include <stddef.h>
#include <stdio.h>

#include "include/hpctoolkit-config.h"

unsigned int
la_version
(
  unsigned int v
)
{
  return v;
}

unsigned int
la_objopen
(
  struct link_map* map,
  Lmid_t lmid,
  uintptr_t* cookie
)
{
  return LA_FLG_BINDTO | LA_FLG_BINDFROM;
}

uintptr_t
la_symbind64
(
  Elf64_Sym *sym,
  unsigned int ndx,
  uintptr_t *refcook,
  uintptr_t *defcook,
  unsigned int *flags,
  const char *symname
)
{
  return sym->st_value;
}

#ifdef HOST_CPU_x86_64

Elf64_Addr
la_x86_64_gnu_pltenter
(
  Elf64_Sym *sym,
  unsigned int ndx,
  uintptr_t *refcook,
  uintptr_t *defcook,
  La_x86_64_regs *regs,
  unsigned int *flags,
  const char *symname,
  long int *framesizep
)
{
  struct link_map* map = (void*)*refcook;
  unsigned long plt_reloc_idx = *((unsigned long *) (regs->lr_rsp-8));
  Elf64_Addr target = sym->st_value;
  
  Elf64_Dyn *dynamic_section = map->l_ld;
  Elf64_Rela *rel = NULL;
  Elf64_Addr *got_entry;
  Elf64_Addr base = map->l_addr;
  for (; dynamic_section->d_tag != DT_NULL; dynamic_section++) {
    if (dynamic_section->d_tag == DT_JMPREL) {
    rel = ((Elf64_Rela *) dynamic_section->d_un.d_ptr) + plt_reloc_idx;
      break;
    }
  }
  if (!rel)
    return target;
  got_entry = (Elf64_Addr *) (rel->r_offset + base);
  *got_entry = target;
  return target;
}

#endif