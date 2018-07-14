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

//******************************************************************************
// system includes
//******************************************************************************

#include <iostream>
#include <vector>
#include <algorithm>

#include <stdint.h>
#include <stdio.h>
#include <string.h>



//******************************************************************************
// local includes
//******************************************************************************

#include "SimpleSymbols.hpp"



//******************************************************************************
// types
//******************************************************************************

struct SimpleSymbolsRepr {
  std::vector<SimpleSymbol*> simple_symbols;
  std::string name;
  bool sorted;
};



//******************************************************************************
// private operations
//******************************************************************************

static bool
compareAddr(SimpleSymbol *s1, SimpleSymbol *s2)
{
  return s1->addr() < s2->addr();
}


static std::string
kindString(SimpleSymbolKind _kind)
{
  std::string kind;
  switch(_kind) {
  case SimpleSymbolKind_Function: kind = "FUN"; break;
  case SimpleSymbolKind_Data: kind = "DAT"; break;
  case SimpleSymbolKind_Unknown: kind = "UNK"; break;
  case SimpleSymbolKind_Other: default: kind = "OTR"; break;
  }
  return kind;
}


static std::string
bindingString(SimpleSymbolBinding _binding)
{
  std::string binding;
  switch(_binding) {
  case SimpleSymbolBinding_Local: binding = "LCL"; break;
  case SimpleSymbolBinding_Global: binding = "GBL"; break;
  case SimpleSymbolBinding_Weak: binding = " WK"; break;
  case SimpleSymbolBinding_Unknown: binding = "UNK"; break;
  case SimpleSymbolBinding_Other: default: binding = "OTR"; break;
  }
  return binding;
}


SimpleSymbol::SimpleSymbol
(
  uint64_t __addr,
  SimpleSymbolKind  __kind,
  SimpleSymbolBinding __binding,
  const char *__name
) : _addr(__addr), _kind(__kind), _binding(__binding), _name(__name)
{
}


void
SimpleSymbol::dump()
{
    std::cout << std::hex << _addr << std::dec
	      << " " << kindString(_kind) << " " << bindingString(_binding)
              << " " << _name << std::endl;
}



//******************************************************************************
// interface operations
//******************************************************************************

SimpleSymbols::SimpleSymbols(const char *name)
{
  R = new struct SimpleSymbolsRepr;
  R->sorted = false;
  R->name = name;
}


const std::string &
SimpleSymbols::name
(
 void
)
{
  return R->name;
}


void
SimpleSymbols::add
(
  uint64_t addr,
  SimpleSymbolKind kind,
  SimpleSymbolBinding binding,
  const char *name
)
{
  R->sorted = false;
  R->simple_symbols.push_back(new SimpleSymbol(addr, kind, binding, name));
}


uint64_t
SimpleSymbols::count()
{
  return R->simple_symbols.size();
}


void
SimpleSymbols::sort()
{
  if (R->sorted == false) {
    if (count() > 1) {
      std::sort (R->simple_symbols.begin(), R->simple_symbols.end(), compareAddr);
    }
    R->sorted = true;
  }
}


void
SimpleSymbols::coalesce(SimpleSymbolsCoalesceCallback coalesce_cb)
{
  sort();
  int nsyms = count();
  int out = 0; // index of symbol to be output
  for (int in = 1; in < nsyms; in++) {
    if (R->simple_symbols[out]->addr() == R->simple_symbols[in]->addr()) {
      // symbol[out] needs to be coalesced with symbol[in]
      // note: coalesce produces the coalesced result in the first argument
      coalesce_cb(R->simple_symbols[out], R->simple_symbols[in]);
    } else {
      R->simple_symbols[++out] = R->simple_symbols[in];
    }
  }
  // erase vector after last symbol to be output
  if (out < nsyms - 1) {
    R->simple_symbols.erase(R->simple_symbols.begin() + out + 1,
                            R->simple_symbols.begin() + nsyms);
  }
}


void
SimpleSymbols::dump()
{
  sort();
  for (auto it = R->simple_symbols.begin();
       it != R->simple_symbols.end(); ++it) {
    (*it)->dump();
  }
}


SimpleSymbol *
SimpleSymbols::find(uint64_t vma)
{
  if (R->simple_symbols.size() <1)
    return NULL;

  sort();

  int first = 0;
  int last = R->simple_symbols.size() - 1;

  if (vma < R->simple_symbols[first]->addr()) {
    return 0;
  }

  if (vma >= R->simple_symbols[last]->addr()) {
    return R->simple_symbols[last];
  }

  for(;;) {
    int mid = (first + last + 1) >> 1;
    if (vma >= R->simple_symbols[mid]->addr()) {
      first = mid;
    } else if (vma < R->simple_symbols[mid]->addr()) {
      last = mid;
    }
    if (last - first <= 1) {
      return R->simple_symbols[first];
    }
  }
  return NULL;
}


bool 
SimpleSymbols::findEnclosingFunction
(
  uint64_t vma, 
  std::string &fnName
)
{
  SimpleSymbol *symbol = find(vma);
  if (symbol) fnName = symbol->name();
  return symbol ? true : false;
}


void
chooseHighestBinding
(
  SimpleSymbol *left,
  const SimpleSymbol *right
)
{
  if (left->binding() < right->binding()) {
    left->setBinding(right->binding());
    left->setKind(right->kind());
    left->setName(right->name());
  } else if (left->binding() == right->binding()) {
    // prefer real names to pseudo-names
    if (left->name()[0] == '<') left->setName(right->name());
    else {
      // prefer low-level names
      if (right->name()[0] == '_') left->setName(right->name());
    }
  }
}
