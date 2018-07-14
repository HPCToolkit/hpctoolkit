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

#ifndef __SIMPLESYMBOLS__
#define __SIMPLESYMBOLS__

//******************************************************************************
// system includes
//******************************************************************************

#include <string>
#include <stdint.h>
#include <set>


//******************************************************************************
// type declarations
//******************************************************************************

typedef enum {
  SimpleSymbolBinding_Unknown = 0,
  SimpleSymbolBinding_Other   = 1,
  SimpleSymbolBinding_Local   = 2,
  SimpleSymbolBinding_Weak    = 3,
  SimpleSymbolBinding_Global  = 4,
} SimpleSymbolBinding;


typedef enum {
  SimpleSymbolKind_Function = 0,
  SimpleSymbolKind_Data     = 1,
  SimpleSymbolKind_Unknown  = 2,
  SimpleSymbolKind_Other    = 3,
} SimpleSymbolKind;


class SimpleSymbol {
public:
  SimpleSymbol(uint64_t __addr, SimpleSymbolKind __kind,
	SimpleSymbolBinding __binding, const char *__name);

  uint64_t addr() const { return _addr; };
  SimpleSymbolKind kind() const { return _kind; };
  SimpleSymbolBinding binding() const { return _binding; };
  const std::string &name() const { return _name; };

  void setName(std::string __name) { _name = __name; }
  void setBinding(SimpleSymbolBinding __binding) { _binding = __binding; }
  void setKind(SimpleSymbolKind __kind) { _kind = __kind; }

  void dump();

private:
  uint64_t _addr;
  SimpleSymbolKind _kind;
  SimpleSymbolBinding _binding;
  std::string _name;
};


// SimpleSymbolsCoalesceCallback must yield its result in left
typedef void
(SimpleSymbolsCoalesceCallback)
(
  SimpleSymbol *left,
  const SimpleSymbol *right
);


// a useful coalescing callback
SimpleSymbolsCoalesceCallback chooseHighestBinding;


class SimpleSymbols {
public:
  SimpleSymbols(const char *name);
  virtual ~SimpleSymbols() {}

  const std::string& name();

  // simply add the element as presented. there is no attempt to incrementally
  // coalesce symbols with the same address. use the coalesce method after
  // all insertions if symbols at the same address should be pruned.
  void add(uint64_t addr, SimpleSymbolKind kind, SimpleSymbolBinding binding,
           const char *name);

  // note: the internal representation is a vector and find causes the
  // vector to be sorted. don't find while still inserting! insert
  // and coalesce at the end to avoid O(n^2) cost.
  SimpleSymbol *find(uint64_t vma);

  // wrapper around find to support name query only
  bool findEnclosingFunction(uint64_t vma, std::string &fnName);

  virtual bool parse(const std::set<std::string> &directorySet, const char *pathname) = 0;

  // invoke the coalesce method to collapse a pair of symbols at the same
  // address into one. for n symbols at the same address, there will be
  // n - 1 callse to coalesce.
  void coalesce(SimpleSymbolsCoalesceCallback coalesce);

  uint64_t count();

  void dump();

private:
  void sort();

  struct SimpleSymbolsRepr *R;
};


class SimpleSymbolsFactory {
public:
  virtual bool match(const char *pathname) = 0;
  virtual SimpleSymbols *create() = 0;

  virtual void id(uint _id) = 0;
  virtual uint id() = 0;

  virtual void fileId(uint _id) = 0;
  virtual uint fileId() = 0;

  virtual const char*unified_name() = 0;
};

#endif
