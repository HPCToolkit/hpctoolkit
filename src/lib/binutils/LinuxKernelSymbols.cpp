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

#include <string.h>
#include <sys/stat.h>

#include <iostream>

//******************************************************************************
// local includes
//******************************************************************************

#include <include/linux_info.h>
#include "LinuxKernelSymbols.hpp"

#include "../support-lean/compress.h"


//******************************************************************************
// local operations
//******************************************************************************

static std::string
getKernelFilename(const std::set<std::string> &directorySet, std::string virtual_name)
{
  std::string path;
  
  // remove the fake symbol < and >
  std::string fname = virtual_name.substr( 1, virtual_name.size()-2 );

  // check if any of the directory in the set has vmlinux.xxxx file
  std::set<std::string>::iterator it;
  for(it = directorySet.begin(); it != directorySet.end(); ++it) {
    std::string dir  = *it;
    path = dir + "/" + KERNEL_SYMBOLS_DIRECTORY + "/" + fname;

    struct stat buffer;

    if (stat(path.c_str(), &buffer) == 0) {
      return path;
    }
  }
  return path;
}

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
LinuxKernelSymbols::parse(const std::set<std::string> &directorySet, const char *pathname)
{
  std::string virtual_path(pathname);
  std::string real_path = getKernelFilename(directorySet, virtual_path);

  if (real_path.empty()) {
    std::cerr << "Warning: cannot find kernel symbols file " << pathname << 
       " ds: "<< directorySet.size() << std::endl;
    perror("LinuxKernelSymbols");
    return false;
  }

  FILE *fp_in = fopen(real_path.c_str(), "r");
  if (fp_in == NULL) {
    // there is nothing critical if we cannot open pseudo load module.
    // we just cannot find the address. 
    return false;
  }

  FILE *fp_deflate = tmpfile();
  if (fp_deflate == NULL) {
    std::cerr << "Error: cannot create temporary file" << std::endl;
    return false;
  }

  FILE *fp_out = fp_deflate;

  enum compress_e decomp_status = compress_inflate(fp_in, fp_deflate);

  // if the decompression is not needed (zlib doesn't exist) we just
  // read the original copied kallsyms
  if (decomp_status == COMPRESS_NONE) {
    fp_out = fp_in;
    fclose(fp_deflate);
  }

  SimpleSymbolBinding binding;
  FILE *fp = fp_out;

  if (fp) {
    rewind(fp);
    size_t len = 4096;
    char *line = (char *) malloc(len);

    for(;;) {
      if (getline(&line, &len, fp) == EOF)
        break; // read a line from the file

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
    if (fp != fp_in)
      fclose(fp_in);
  }
  coalesce(chooseHighestBinding);

  return count() > 0;
}


bool
LinuxKernelSymbolsFactory::match(const char *pathname)
{
  if (pathname == NULL)
    return false;

  bool prefix_correct = pathname[0] == '<';
  bool suffix_correct = pathname[strlen(pathname)-1] == '>';
  bool name_correct   = 0<=strncmp(pathname+1, LINUX_KERNEL_NAME_REAL, strlen(LINUX_KERNEL_NAME_REAL));

  return prefix_correct && suffix_correct && name_correct;
}


SimpleSymbols *
LinuxKernelSymbolsFactory::create(void)
{
  if (m_kernelSymbol) {
    return m_kernelSymbol;
  }
  m_kernelSymbol = new LinuxKernelSymbols;
  return m_kernelSymbol;
}

void
LinuxKernelSymbolsFactory::id(uint _id)
{
  // only accept the first ID, and throw the others
  if (m_id_status == UNINITIALIZED) {
    m_id = _id;
    m_id_status = INITIALIZED;
  }
}

uint
LinuxKernelSymbolsFactory::id()
{
  return m_id;
}

void
LinuxKernelSymbolsFactory::fileId(uint _id)
{
  // only accept the first ID, and throw the others
  if (m_fileId_status == UNINITIALIZED) {
    m_fileId = _id;
    m_fileId_status = INITIALIZED;
  }
}

uint
LinuxKernelSymbolsFactory::fileId()
{
  return m_fileId;
}



const char*
LinuxKernelSymbolsFactory::unified_name()
{
  return LINUX_KERNEL_NAME;
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
