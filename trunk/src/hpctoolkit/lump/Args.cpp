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

#ifdef NO_STD_CHEADERS
# include <stdlib.h>
#else
# include <cstdlib>
using std::strtol; // For compatibility with non-std C headers
#endif


#include <unistd.h> // for 'getopt'
#include <errno.h>

//*************************** User Include Files ****************************

#include "Args.h"

#include <lib/support/String.h>
#include <lib/support/Trace.h>

//*************************** Forward Declarations **************************

using std::cerr;
using std::endl;

//***************************************************************************

static const char* usage_summary =
"[options] <loadmodule>\n";

static const char* usage_details =
"Dumps info about contents of <loadmodule> to stdout.  <loadmodule> may be\n"
"either an executable or DSO.\n"
"\n"
"By default, section, procedure and instruction lists are dumped.\n"
"  -s: Instead of the default, dump symbolic info (file, func, line).\n"
"  -l: By default, DSOs will be 'loaded' at 0x0.  Use this option to\n"
"      specify a different load address.  Addresses may be in base 10, 8\n"
"      (prefix '0') or 16 (prefix '0x').\n"
"      [NOT FULLY IMPLEMENTED]\n"
"  -d: Debug. (Give multiple times to increase debug level.)\n";


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
  symbolicDump = false;
  loadAddr = 0x0;
  debugMode = false;
}


Args::~Args()
{
}


void 
Args::PrintUsage(std::ostream& os) const
{
  os << "Usage: " << cmd << " " << usage_summary << endl
     << usage_details << endl;
} 


void
Args::Parse(int argc, const char* const argv[])
{
  cmd = argv[0]; 

  // -------------------------------------------------------
  // Parse the command line
  // -------------------------------------------------------
  extern char *optarg;
  extern int optind;
  bool error = false;
  trace = 0;
  int c;
  while ((c = getopt(argc, (char**)argv, "sl:Dd")) != EOF) {
    switch (c) {
    case 's': { 
      symbolicDump = true;
      break; 
    }
    case 'l': {
      if (optarg == NULL) { 
	error = true; 
	break;
      }
      
      errno = 0;
      long l = strtol(optarg, NULL, 0 /* base: dec, hex, or oct */);
      if (l <= 0 || errno != 0) {
	cerr << "Invalid address given to -r\n";
	error = true;
      }
      loadAddr = (Addr)l;

      break; 
    }
    case 'D': {
      debugMode = true; 
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

  
  if (error) {
    PrintUsage(cerr);
    exit(1); 
  }
}


void 
Args::Dump(std::ostream& os) const
{
  os << "Args.cmd= " << cmd << endl; 
  os << "Args.symbolicDump= " << symbolicDump << endl;
  os << "Args.debugMode= " << debugMode << endl;
  os << "Args.inputFile= " << inputFile << endl;
  os << "::trace " << ::trace << endl; 
}

void 
Args::DDump() const
{
  Dump(std::cerr);
}
