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
// Copyright ((c)) 2002-2021, Rice University
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

// This file makes a dot (graphviz) file for the Control-Flow Graph
// for each function in the binary.  This file is split off from
// Struct.cpp for two reasons.
//
//  1. ParseAPI does not work on cuda binaries.
//
//  2. It's easier to add command-line options for things like subset
//  of functions and addresses as a separate program, not part of
//  hpcstuct.
//
// Note: we can't use our library functions due to build order.  Need
// to move this to src/tool if that is necessary.

//***************************************************************************

#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <string>
#include <ostream>
#include <fstream>

#include <CFG.h>
#include <CodeObject.h>
#include <CodeSource.h>
#include <Function.h>
#include <Symtab.h>
#include <Instruction.h>

#include <lib/binutils/ElfHelper.hpp>
#include <lib/binutils/InputFile.hpp>

#include <lib/banal/Struct-Inline.hpp>

#include <lib/banal/gpu/ReadCudaCFG.hpp>

#ifdef ENABLE_IGC
#include <lib/banal/gpu/ReadIntelCFG.hpp>
#endif // ENABLE_IGC

#include <include/hpctoolkit-config.h>

#ifdef ENABLE_OPENMP
#include <omp.h>
#endif

using namespace Dyninst;
using namespace SymtabAPI;
using namespace ParseAPI;
using namespace std;

class Options {
public:
  char * filename;
  int  jobs;
  char *func;

  Options()
  {
    filename = (char *) "";
    jobs = 1;
    func = 0;
  }
};

//----------------------------------------------------------------------

// Make a dot (graphviz) file for the Control-Flow Graph for each
// procedure and write to the ostream 'dotFile'.
//
static void
makeDotFile(ofstream * dotFile, CodeObject * code_obj, const char *only_func)
{
  const CodeObject::funclist & funcList = code_obj->funcs();

  for (auto fit = funcList.begin(); fit != funcList.end(); ++fit)
  {
    ParseAPI::Function * func = *fit;
    
    if (only_func) {
      // if only_func is a non-NULL character string, only produce 
      // output for this function
      if (strcmp(only_func, func->name().c_str()) != 0) {
	// name doesn't match only_func
	continue;
      }
    }

    map <Block *, int> blockNum;
    map <Block *, int>::iterator mit;
    int num;

    *dotFile << "// --------------------------------------------------\n"
	     << "// Procedure: '" << func->name() << "'\n\n"
	     << "digraph \"" << func->name() << "\" {\n"
	     << "  1 [ label=\"start\" shape=\"diamond\" ];\n";

    const ParseAPI::Function::blocklist & blist = func->blocks();

    // write the list of nodes (blocks)
    num = 1;
    for (auto bit = blist.begin(); bit != blist.end(); ++bit) {
      Block * block = *bit;
      num++;

      blockNum[block] = num;

      *dotFile << "  " << num << " [ label=\"0x" << hex << block->start()
	       << "\\n0x" << block->end() << dec << "\" ];\n";

    }
    int endNum = num + 1;
    *dotFile << "  " << endNum << " [ label=\"end\" shape=\"diamond\" ];\n";

    // in parseAPI, functions have a unique entry point
    mit = blockNum.find(func->entry());
    if (mit != blockNum.end()) {
      *dotFile << "  1 -> " << mit->second << ";\n";
    }

    // write the list of internal edges
    num = 1;
    for (auto bit = blist.begin(); bit != blist.end(); ++bit) {
      Block * block = *bit;
      const ParseAPI::Block::edgelist & elist = block->targets();
      num++;

      for (auto eit = elist.begin(); eit != elist.end(); ++eit) {
	mit = blockNum.find((*eit)->trg());
	if (mit != blockNum.end()) {
	  *dotFile << "  " << num << " -> " << mit->second << ";\n";
	}
      }
    }

    // add any exit edges
    const ParseAPI::Function::const_blocklist & eblist = func->exitBlocks();
    for (auto bit = eblist.begin(); bit != eblist.end(); ++bit) {
      Block * block = *bit;
      mit = blockNum.find(block);
      if (mit != blockNum.end()) {
	*dotFile << "  " << mit->second << " -> " << endNum << ";\n";
      }
    }

    *dotFile << "}\n" << endl;
  }
}

//----------------------------------------------------------------------

static void
usage(string mesg)
{
  cout << "usage:  dotgraph  [options]...  filename\n\n"
       << "options:\n"
       << "  -f func      only produce output for function 'func'\n"
       << "  -j num       use num threads for ParseAPI::parse()\n"
       << "  -h, --help   display usage message and exit\n"
       << "\n";

  if (! mesg.empty()) {
    errx(1, "%s", mesg.c_str());
  }
  exit(0);
}

static void
getOptions(int argc, char **argv, Options & opts)
{
  if (argc < 2) {
    usage("");
  }

  int n = 1;
  while (n < argc) {
    string arg(argv[n]);

    if (arg == "-h" || arg == "-help" || arg == "--help") {
      usage("");
    }
    else if (arg == "-f") {
      if (n + 1 >= argc) {
	usage("missing arg for -f");
      }
      opts.func = argv[n + 1];
      n += 2;
    }
    else if (arg == "-j") {
      if (n + 1 >= argc) {
	usage("missing arg for -j");
      }
      opts.jobs = atoi(argv[n + 1]);
      if (opts.jobs <= 0) {
	errx(1, "bad arg for -j: %s", argv[n + 1]);
      }
      n += 2;
    }
    else if (arg[0] == '-') {
      usage("invalid option: " + arg);
    }
    else {
      break;
    }
  }

  // filename (required)
  if (n < argc) {
    opts.filename = argv[n];
  }
  else {
    usage("missing file name");
  }

#ifndef ENABLE_OPENMP
  opts.jobs = 1;
#endif
}

//----------------------------------------------------------------------

// Usage:  dotgraph  [-j num]  <binary>
// output:  <binary>.dot.<0-n>
int
main(int argc, char **argv)
{
  Options opts;

  getOptions(argc, argv, opts);

  InputFile inputFile;
  auto filename = std::string(opts.filename);
  inputFile.openFile(filename, InputFileError_Error);
  ElfFileVector * elfFileVector = inputFile.fileVector();

  // Output structure files under the current directory
  auto search_path = "./";
  for (uint i = 0; i < elfFileVector->size(); i++) {
    auto * elfFile = (*elfFileVector)[i];
    // open <filename>.dot.<i> for output
    char * base = basename(strdup(opts.filename));
    string dotname = string(base) + ".dot." + std::to_string(i);
    ofstream dotFile;

    dotFile.open(dotname, ios::out | ios::trunc);
    if (! dotFile.is_open()) {
      errx(1, "unable to open for output: %s", dotname.c_str());
    }

#ifdef ENABLE_OPENMP
    omp_set_num_threads(1);
#endif

    // read input file and parse
    Symtab * symtab = Inline::openSymtab(elfFile);
    if (symtab == NULL) {
      errx(1, "Inline::openSymtab failed: %s", elfFile->getFileName().c_str());
    }

    bool cuda_file = (symtab)->getArchitecture() == Dyninst::Arch_cuda;
    bool intel_file = elfFile->isIntelGPUFile();

#ifdef ENABLE_OPENMP
    omp_set_num_threads(opts.jobs);
#endif

    bool parsable = true;
    CodeSource *code_src = NULL;
    CodeObject *code_obj = NULL;

    if (cuda_file) { // don't run parseapi on cuda binary
      parsable = readCudaCFG(search_path, elfFile, symtab, true, &code_src, &code_obj);
    } else if (intel_file) { // don't run parseapi on intel binary
      #ifdef ENABLE_IGC
      parsable = readIntelCFG(search_path, elfFile, symtab, true, &code_src, &code_obj);
      #endif // ENABLE_IGC
    } else {
      code_src = new SymtabCodeSource(symtab);
      code_obj = new CodeObject(code_src);
      code_obj->parse();
    }

    if (parsable) {
      makeDotFile(&dotFile, code_obj, opts.func);
    }
  }

  return 0;
}
