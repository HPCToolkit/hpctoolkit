// -*-Mode: C++;-*-
// $Id$
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

static const char* version_info=
#include <include/HPCToolkitVersionInfo.h>

static const char* usage_summary =
"[options] <binary>\n";

static const char* usage_details =
"bloop analyzes the application binary <binary>, recovers information about\n"
"its source-line loop-nesting structure, and generates an XML scope tree (of\n"
"type PGM) to stdout.  It uses debugging information to gather source-line\n"
"data; see caveats below for common problems.\n"
"\n"
"Options:\n"
"  -v            Verbose: generate progress messages\n"
"  -n            Turn off scope tree normalization\n"
"  -c            Generate compact output, eliminating extra white space\n"
"  -p <pathlist> Ensure that scope tree only contains files found in the\n"
"                colon-separated <pathlist>\n"
"  -V            Print version information\n"
"  -h            Print this help\n"
" [-d            Debug default mode; not for general use.]\n"
"\n"
"Debug mode: Dump binary information. Not for general use.\n"
"  -D  Turn on debug mode\n"
" [-d  Debug debug mode.]\n"
"\n"
"Caveats:\n"
"* <binary> should be compiled with as much debugging info as possible (e.g.\n"
"  -g3 for some compilers). When using the Sun compiler, place debugging\n"
"  info _in_ the binary (-xs).\n"
"* C++ mangling is compiler specific. bloop tries both the platform's and\n"
"  GNU's demangler, but if <binary> was produced with a proprietary compiler\n"
"  demangling will likely be unsuccessful. (Also, cross-platform usage.)\n";

#if 0 // FIXME: '[-m <pcmap>]'
"  -m: Create and write a [PC -> source-line] map to <pcmap>.\n"
"      The map is extended with loop analysis information and  \n"
"      can be used to improve the output of 'xProf'.\n"
"      [*Not fully implemented.*]\n"
#endif     


//***************************************************************************
// Args
//***************************************************************************

Args::Args()
{
  Ctor();
}

Args::Args(int argc, const char* const argv[])
{
  Ctor();
  Parse(argc, argv);
}

void
Args::Ctor()
{
  debugMode = false;
  prettyPrintOutput = true;
  normalizeScopeTree = true;
  verboseMode = false;
}


Args::~Args()
{
}


void 
Args::PrintVersion(std::ostream& os) const
{
  os << cmd << ": " << version_info << endl;
}


void 
Args::PrintUsage(std::ostream& os) const
{
  os << "Usage: " << cmd << " " << usage_summary << endl
     << usage_details << endl;
} 


void 
Args::PrintError(std::ostream& os, const char* msg) const
{
  os << cmd << ": " << msg << endl
     << "Try `" << cmd << " -h' for more information." << endl;
}


void
Args::Parse(int argc, const char* const argv[])
{
  // FIXME: eraxxon: drop my new argument parser in here
  cmd = argv[0]; 

  bool printHelp = false;
  bool printVersion = false;

  // -------------------------------------------------------
  // Parse the command line
  // -------------------------------------------------------
  extern char *optarg;
  extern int optind;
  bool error = false;
  trace = 0;
  int c;
  while ((c = getopt(argc, (char**)argv, "vVhDm:np:cd")) != EOF) {
    switch (c) {
    case 'D': {
      debugMode = true; 
      break; 
    }
    case 'v':{
      verboseMode = true;
      break;
    }
    case 'V': { 
      printVersion = true;
      break; 
    }
    case 'h': { 
      printHelp = true;
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

  // -------------------------------------------------------
  // Sift through results, checking for semantic errors
  // -------------------------------------------------------

  // Special options that should be checked first
  if (printHelp) {
    PrintUsage(cerr);
    exit(1);
  }
  
  if (printVersion) {
    PrintVersion(cerr);
    exit(1);
  }

  error = error || (optind != argc-1); 
  if (!error) {
    inputFile = argv[optind];
  } 
  
  if (error) {
    PrintError(cerr, "Error parsing command line"); 
    exit(1); 
  }
}


void 
Args::Dump(std::ostream& os) const
{
  os << "Args.cmd= " << cmd << endl; 
  os << "Args.debugMode= " << debugMode << endl;
  os << "Args.pcMapFile= " << pcMapFile << endl;
  os << "Args.prettyPrintOutput= " << prettyPrintOutput << endl;
  os << "Args.normalizeScopeTree= " << normalizeScopeTree << endl;
  os << "Args.canonicalPathList= " << canonicalPathList << endl;
  os << "Args.inputFile= " << inputFile << endl;
  os << "::trace " << ::trace << endl; 
}

void 
Args::DDump() const
{
  Dump(std::cerr);
}


//***************************************************************************

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

