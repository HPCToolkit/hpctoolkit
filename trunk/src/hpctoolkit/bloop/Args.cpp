// $Id$
// -*-C++-*-
// * BeginRiceCopyright *****************************************************
// 
// Copyright ((c)) 2002, Rice University 
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
//    Args.C
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

//************************* System Include Files ****************************

#include <iostream>

#include <unistd.h> // for 'getopt'

//*************************** User Include Files ****************************

#include "Args.h"

#include <lib/support/String.h>
#include <lib/support/Trace.h>

//*************************** Forward Declarations **************************

using std::cerr;
using std::endl;

//***************************************************************************

const char* hpctoolsVerInfo=
#include <include/HPCToolkitVersionInfo.h>

void Args::Version()
{
  cerr << cmd << ": " << hpctoolsVerInfo << endl;
}

void Args::Usage()
{
  cerr
    << "Usage: " << endl
    << "  " << cmd << " [-V] [-n] [-p <pathlist>] [-c] <binary>\n"
    << "  " << cmd << " -D <binary>\n"
    << endl;
  cerr
    << "Default mode: Analyzes the application binary <binary> and recovers\n"
    << "information about source line loop nesting structure.  Generates an\n"
    << "XML scope tree (of type PGM) of the binary's contents to stdout.\n"
    << "\n"
    << "'bloop' analyzes C, C++ and Fortran binaries from a number of\n"
    << "different platforms, relying on debugging information for source\n"
    << "line information.  It currently supports the following targets and\n"
    << "compilers:\n"
    << "  alpha-OSF1:  GCC 2.9x, 3.x, Compaq Compilers 6.x\n"
    << "  i686-Linux:  GCC 2.9x, 3.x\n"
    << "  ia64-Linux:  GCC 2.9x, 3.x, Intel Compilers 6.x, 7.x\n"
    << "  mips-IRIX64: GCC 2.9x, 3.x, SGI MIPSpro Compilers 7.x \n"
    << "  sparc-SunOS: GCC 2.9x, 3.x\n"
    << "One caveat to supporting multiple targets is that C++ mangling\n"
    << "is often compiler specific.  Both the current platform's demangler\n"
    << "and GNU's demangler are used in attempts to demangle names.  But if\n"
    << "<binary> was compiled on another platform with a proprietary\n"
    << "compiler, there is a good chance demangling will be unsuccessful.\n"
    << "\n"
    << "In order to ensure accurate reporting, <binary> should be compiled\n"
    << "with as much debugging information as possible. \n"
    << "  \n"
#if 0 // FIXME: '[-m <pcmap>]'
    << "  -m: Create and write a [PC -> source-line] map to <pcmap>.\n"
    << "      The map is extended with loop analysis information and  \n"
    << "      can be used to improve the output of 'xProf'.\n"
    << "      [*Not fully implemented.*]\n"
#endif     
    << "  -V: print version information\n"
    << "  -n: Turn off scope tree 'normalization'\n"
    << "  -p: Ensure the scope tree only contains those files found in\n"
    << "      the colon separated <pathlist>\n"
    << "  -c: Print output in compact form without extra white space\n"
    << " [-d: Debug default mode; not for general use.]\n"
    << "\n"
    << "Debug mode: Prints an unprocessed section, procedure, and \n"
    << "instruction list of <binary> to stdout.\n"
    << "  -D: Turn on debug mode\n"
    << " [-d: Debug debug mode.]\n"
    << endl;
} 

Args::Args(int argc, char* const* argv)
{
  cmd = argv[0]; 

  bool printVersion = false;
  debugMode = false;
  prettyPrintOutput = true;
  normalizeScopeTree = true;
  
  extern char *optarg;
  extern int optind;
  bool error = false;
  trace = 0;
  int c;
  while ((c = getopt(argc, argv, "VDm:np:cd")) != EOF) {
    switch (c) {
    case 'D': {
      debugMode = true; 
      break; 
    }
    case 'V': { 
      printVersion = true;
      break; 
    }
    case 'm': {
      // A non-null value of 'pcMapFile' indicates it has been set
      if (optarg == NULL) { error = true; }
      pcMapFile = optarg;
      break;
    }
    case 'n': {
      normalizeScopeTree = false;
      break; 
    }
    case 'p': {
      // A non-null value of 'canonicalPathList' indicates it has been set
      if (optarg == NULL) { error = true; }
      canonicalPathList = optarg;
      break; 
    }
    case 'c': {
      prettyPrintOutput = false;
      break; 
    }
    case 'd': { // debug 
      trace++; 
      break; 
    }
    case ':':
    case '?': { // error
      error = true; 
      break; 
    }
    }
  }
  error = error || (optind != argc-1); 
  if (!error) {
    inputFile = argv[optind];
  } 

  IFTRACE << "Args.cmd= " << cmd << endl; 
  IFTRACE << "Args.debugMode= " << debugMode << endl;
  IFTRACE << "Args.pcMapFile= " << pcMapFile << endl;
  IFTRACE << "Args.prettyPrintOutput= " << prettyPrintOutput << endl;
  IFTRACE << "Args.normalizeScopeTree= " << normalizeScopeTree << endl;
  IFTRACE << "Args.canonicalPathList= " << canonicalPathList << endl;
  IFTRACE << "Args.inputFile= " << inputFile << endl;
  IFTRACE << "::trace " << ::trace << endl; 
  
  if (printVersion) {
    Version();
    exit(1);
  }
  
  if (error) {
    Usage(); 
    exit(1); 
  }
}

#if 0
#include <dirent.h>
void Args::setBloopHome() 
{
  char * home = getenv(HPCTOOLS); 
  if (home == NULL) {
    cerr << "Error: Please set your " << HPCTOOLS << " environment variable."
	 << endl; 
    exit(1); 
  } 
   
  // chop of trailing slashes 
  int len = strlen(home); 
  if (home[len-1] == '/') home[--len] = 0; 
   
  DIR *fp = opendir(home); 
  if (fp == NULL) {
    cerr << "Error: " << home << " is not a directory" << endl; 
    exit(1); 
  } 
  closedir(fp); 
  bloopHome = String(home);
} 
#endif  

