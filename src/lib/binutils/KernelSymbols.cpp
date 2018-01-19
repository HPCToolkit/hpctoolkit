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
// system include files
//******************************************************************************

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <algorithm>



//******************************************************************************
// local include files
//******************************************************************************

#include <include/linux_info.h>
#include "KernelSymbols.hpp"

#include "decompress.h"


//******************************************************************************
// macros
//******************************************************************************

#define MAX_CHARS_INLINE 4096

//******************************************************************************
// types
//******************************************************************************

class KernelSymbol {
public:
  KernelSymbol(uint64_t __addr, char __type, const char *__name); 
  std::string &name();
  uint64_t addr();
  void dump();

private:
  uint64_t _addr;
  char _type;
  std::string _name;
};


struct KernelSymbolsRepr {
  std::vector<KernelSymbol*> kernel_symbols; 
};



//******************************************************************************
// private operations
//******************************************************************************
KernelSymbol::KernelSymbol(
  uint64_t __addr, 
  char __type, 
  const char *__name) 
  : _addr(__addr), _type(__type), _name(__name) 
{
}
  
void  
KernelSymbol::dump() 
{
    std::cout << std::hex << _addr << std::dec 
	      << " " << _type << " " << _name << std::endl;
}


std::string &
KernelSymbol::name()
{
  return _name;
}


uint64_t
KernelSymbol::addr()
{
  return _addr;
}


static bool 
compare(KernelSymbol *s1, KernelSymbol *s2)
{
  return s1->addr() < s2->addr(); 
}



//******************************************************************************
// interface operations
//******************************************************************************

KernelSymbols::KernelSymbols() 
{
  R = new struct KernelSymbolsRepr;
}


bool
KernelSymbols::parseLinuxKernelSymbols(std::string &filename)
{
  FILE *fp_in = fopen(filename.c_str(), "r");
  if (fp_in == NULL)
    return false;

  FILE *fp_deflate = tmpfile();
  if (fp_deflate == NULL)
    return false;

  FILE *fp_out = fp_deflate;

  enum decompress_e decomp_status = compress_inflate(fp_in, fp_deflate);

  // if the decompression is not needed (zlib doesn't exist) we just
  // read the original copied kallsyms
  if (decomp_status == DECOMPRESS_NONE) {
    fp_out = fp_in;
    fclose(fp_deflate);
  }

  if (fp_out != NULL) {
    fseek(fp_out, 0, SEEK_SET);

    size_t len = MAX_CHARS_INLINE;
    char *line = (char*) malloc(sizeof(char) * len);

    if (line == NULL)
      return false;
    
    for(;;) {
      if (getline(&line, &len, fp_out) == EOF) break; // read a line from the file

      // parse the line into 3 or 4 parts
      char type;
      void *addr;
      char name[MAX_CHARS_INLINE];
      char module[MAX_CHARS_INLINE];
      module[0] = 0; // initialize module to null in case it is missing
      int result = sscanf(line, "%p %c %s %s\n", &addr, &type, name, module);

      if (result < 3) break;

      switch(type) {
      case 't':
      case 'T':
        // if module is non-empty, append it to name 
      	if (strlen(module) > 0) {
           strcat(name, " "); 
           strcat(name, module); 
        }
        // add name to the set of function symbols
        R->kernel_symbols.push_back(new KernelSymbol((uint64_t) addr, type, name));
        break;
      default:
        break;
      }
    }
    if (line != NULL) free(line);

    fclose(fp_in);
    if (fp_in != fp_out)
      fclose(fp_out);
  }
  int size = R->kernel_symbols.size();

  if (size > 1) {
   std::sort (R->kernel_symbols.begin(), R->kernel_symbols.end(), compare);
  }

  return size > 0;
}


bool
KernelSymbols::find(uint64_t vma, std::string &fnname)
{
  int first = 0;
  int last = R->kernel_symbols.size() - 1;

  if (last < first)
    return false;

  if (vma < R->kernel_symbols[first]->addr()) {
    return false;
  }

  if (vma >= R->kernel_symbols[last]->addr()) {
    fnname = R->kernel_symbols[last]->name();
    return true;
  }

  for(;;) { 
    int mid = (first + last + 1) >> 1;
    if (vma >= R->kernel_symbols[mid]->addr()) {
      first = mid;
    } else if (vma < R->kernel_symbols[mid]->addr()) {
      last = mid;
    } 
    if (last - first <= 1) {
      fnname = R->kernel_symbols[first]->name();
      return true;
    }
  }
  return false;
}



void 
KernelSymbols::dump()
{
  for (auto it = R->kernel_symbols.begin(); it != R->kernel_symbols.end(); ++it) {
    (*it)->dump();
  }
}



//******************************************************************************
// unit test
//******************************************************************************
// #define UNIT_TEST

#ifdef UNIT_TEST

int main(int argc, char **argv)
{
  KernelSymbols syms;
  syms.parseLinuxKernelSymbols();
  syms.dump();

  uint64_t addr = 0;
  if (argc == 2) {
    sscanf(argv[1], "%p", &addr);
    std::string name;
    bool result = syms.find(addr, name);
    std::cout << "Lookup " << std::hex << "0x" << addr << std::dec 
	      << " (" << result << ")" << " --> " << name << std::endl; 
  }
}

#endif
