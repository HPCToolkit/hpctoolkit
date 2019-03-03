// -*-Mode: C++;-*-

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
// Copyright ((c)) 2002-2019, Rice University
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

//******************************************************************************
// system includes
//******************************************************************************

#include <iostream>
#include <string.h>



//******************************************************************************
// local includes
//******************************************************************************

#include <lib/prof-lean/vdso.h>

#include "dso_symbols.h"
#include "VdsoSymbols.hpp"



//******************************************************************************
// private functions
//******************************************************************************

static SimpleSymbolBinding
binding_xlate
(
 dso_symbol_bind_t bind
)
{
  switch(bind) {
  case dso_symbol_bind_local:  return SimpleSymbolBinding_Local;
  case dso_symbol_bind_global: return SimpleSymbolBinding_Global;
  case dso_symbol_bind_weak:   return SimpleSymbolBinding_Weak;
  case dso_symbol_bind_other:  return SimpleSymbolBinding_Other;
  }
}


static void
note_symbol
(
 const char *sym_name,
 int64_t sym_addr,
 dso_symbol_bind_t binding,
 void *callback_arg
)
{
  VdsoSymbols *vdso_syms = (VdsoSymbols *) callback_arg;
  SimpleSymbolBinding btype = binding_xlate(binding);

  // add name to the set of function symbols
  vdso_syms->add((uint64_t) sym_addr, SimpleSymbolKind_Function,
                 btype, sym_name);
}



//******************************************************************************
// interface operations
//******************************************************************************

VdsoSymbols::VdsoSymbols
(
 void
) : SimpleSymbols(VDSO_SEGMENT_NAME_SHORT)
{
}


bool
VdsoSymbols::parse(const char *pathname)
{
  int success = (pathname ? 
                 dso_symbols(pathname, note_symbol, this) : 
                 dso_symbols_vdso(note_symbol, this)) ; 

  if (success) coalesce(chooseHighestBinding);

#ifdef DEBUG_VDSOSYMBOLS
  dump();
#endif

  return success ? true : false;
}


bool
VdsoSymbolsFactory::match
(
  const char *pathname
)
{
  const char *slash = strrchr(pathname, '/');
  const char *basename = (slash ? slash + 1 : pathname);
  return strcmp(basename, VDSO_SEGMENT_NAME_SHORT) == 0;
}


SimpleSymbols *
VdsoSymbolsFactory::create
(
 void
)
{
    return new VdsoSymbols;
}



//******************************************************************************
// unit test
//******************************************************************************
// #define UNIT_TEST

#ifdef UNIT_TEST

int main(int argc, char **argv)
{
  VdsoSymbols syms;
  syms.parse();
  syms.dump();

  uint64_t addr = 0;
  if (argc == 2) {
    sscanf(argv[1], "%lx", &addr);
    std::string name;
    SimpleSymbol *sym = syms.find(addr);
    bool result = (sym != 0);
    if (result) name = sym->name();
    std::cout << "Lookup " << std::hex << "0x" << addr << std::dec
	      << " (" << result << ")" << " --> " << name << std::endl;
  }
}

#endif
