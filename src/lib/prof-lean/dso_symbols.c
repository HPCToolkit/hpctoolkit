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
// Copyright ((c)) 2002-2017, Rice University
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
//   dso_symbols.c
//
// Purpose:
//   implementation that extracts dynamic symbols from a shared library
//
// Description:
//   extract dynamic symbols from a shared library. this needs to be done
//   at runtime to understand VDSOs.
//
//
//***************************************************************************


//******************************************************************************
// system includes
//******************************************************************************

#include <stdio.h>
#include <stdlib.h>



//******************************************************************************
// local includes
//******************************************************************************

#include <libelf.h>
#include <gelf.h>

#include <vdso.h>
#include <dso_symbols.h>



//******************************************************************************
// private functions
//******************************************************************************

static int
dso_symbol_binding
(
 GElf_Sym *s
)
{
  switch(GELF_ST_BIND(s->st_info)){
  case STB_LOCAL:  return dso_symbol_bind_local;
  case STB_GLOBAL: return dso_symbol_bind_global;
  case STB_WEAK:   return dso_symbol_bind_weak;
  default:         return dso_symbol_bind_other;
  }
}


static int
dso_symbols_internal
(
 Elf *elf,
 dso_symbols_symbol_callback_t note_symbol, // callback
 void *callback_arg                         // closure state
)
{
  int status_ok = 0;
  int nsymbols;
  if (elf) {
    Elf_Scn *scn = NULL;
    GElf_Shdr shdr;
    while ((scn = elf_nextscn(elf, scn)) != NULL) {
      if (!gelf_getshdr(scn, &shdr)) {
	// failed to get section header
	break;
      }
      if (shdr.sh_type == SHT_DYNSYM) {
	// found dynamic symbol section
	if (shdr.sh_entsize > 0) { // avoid divide by 0
	  nsymbols = shdr.sh_size / shdr.sh_entsize;
	  if (nsymbols > 0) status_ok = 1;
	}
	// always break regardless of status since only
	// one section of dynamic symbols is allowed
	break;
      }
    }
    if (status_ok) {
      status_ok = 0; // no symbols found yet
      Elf_Data *datap = elf_getdata(scn, NULL);
      if (datap) {
	int i;
	for (i = 0; i < nsymbols; i++) {
	  GElf_Sym sym;
	  GElf_Sym *symp = gelf_getsym(datap, i, &sym);
	  if (symp) { // symbol properly read
	    dso_symbol_bind_t binding = dso_symbol_binding(&sym);
	    int symtype = GELF_ST_TYPE(sym.st_info);
	    if (sym.st_shndx == SHN_UNDEF) continue;
	    if (binding == dso_symbol_bind_other) continue;
	    switch(symtype) {
	    case STT_FUNC:
	      // function symbols are definitely of interest
	    case STT_NOTYPE:
	      // for [vdso] LINUX 2.6.15 on ppc64 
	      // functions have STT_NOTYPE. what nonsense!
	      {
		int64_t addr_signed = (int64_t) sym.st_value;
		note_symbol(elf_strptr(elf, shdr.sh_link, sym.st_name),
			    addr_signed, binding, callback_arg);
		status_ok = 1; // at least one symbol found
	      }
	    default:
	      break;
	    }
	  }
	}
      }
    }
  }
  return status_ok;
}



//******************************************************************************
// interface functions
//******************************************************************************

int
dso_symbols_vdso
(
 dso_symbols_symbol_callback_t note_symbol, // callback
 void *callback_arg                         // closure state
)
{
  int status_ok = 0;
  char *vdso_start = (char *) vdso_segment_addr();
  if (vdso_start) {
    elf_version(EV_CURRENT);
    size_t vdso_len = vdso_segment_len();
    Elf *vdso_elf = elf_memory(vdso_start, vdso_len);
    if (vdso_elf) {
      status_ok = dso_symbols_internal(vdso_elf, note_symbol, callback_arg);
      elf_end(vdso_elf);
    }
  }
  return status_ok;
}


int
dso_symbols
(
 const char *filename,
 dso_symbols_symbol_callback_t note_symbol, // callback
 void *callback_arg                         // closure state
)
{
  int status_ok = 0;
  FILE *file = fopen(filename, "r");
  if (file) {
    elf_version(EV_CURRENT);
    Elf *elf = elf_begin(fileno(file), ELF_C_READ, (Elf *) 0);
    if (elf) {
      status_ok = dso_symbols_internal(elf, note_symbol, callback_arg);
      elf_end(elf);
    }
  }
  return status_ok;
}
