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

#include <string.h>
#include <iostream>



//******************************************************************************
// local includes
//******************************************************************************

#include <include/linux_info.h>
#include "LinuxKernelSymbols.hpp"



//******************************************************************************
// interface operations
//******************************************************************************

LinuxKernelSymbols::LinuxKernelSymbols
(
 void
) : SimpleSymbols(LINUX_KERNEL_NAME)
{
}


bool
LinuxKernelSymbols::parse(const char *pathname)
{
  SimpleSymbolBinding binding;
  FILE *fp = fopen(pathname, "r");

  if (fp) {
    size_t len = 4096;
    char *line = (char *) malloc(len);

    for(;;) {
      if (getline(&line, &len, fp) == EOF) break; // read a line from the file

      // parse the line into 3 or 4 parts
      char type;
      void *addr;
      char name[4096];
      char module[4096];
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

        binding = ((type == 't') ?
	           SimpleSymbolBinding_Local :
	           SimpleSymbolBinding_Global);

        // add name to the set of function symbols
        add((uint64_t) addr, SimpleSymbolKind_Function, binding, name);
        break;
      default:
	break;
      }
    }
    fclose(fp);
  }

  coalesce(chooseHighestBinding);

  return count() > 0;
}


bool
LinuxKernelSymbolsFactory::match
(
 const char *pathname
)
{
  const char *slash = strrchr(pathname, '/');
  const char *basename = (slash ? slash + 1 : pathname);
  return strcmp(basename, LINUX_KERNEL_NAME) == 0;
}


SimpleSymbols *
LinuxKernelSymbolsFactory::create
(
 void
)
{
  return new LinuxKernelSymbols;
}



//******************************************************************************
// unit test
//******************************************************************************
// #define UNIT_TEST

#ifdef UNIT_TEST

int main(int argc, char **argv)
{
  LinuxKernelSymbols syms;
  syms.parse(LINUX_KERNEL_SYMBOL_FILE);
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
