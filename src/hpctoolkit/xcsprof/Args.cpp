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
    << "  " << cmd << " [-V] <binary> <profile>\n"
    << endl;
  cerr
    << "Converts various types of profile output into the CSPROFILE format,\n"
    << "FIXME.  Output is sent to stdout.\n"
    << "\n"
#if 0  // FIXME '[-m <bloop-pcmap>]'
    << "If no <bloop-pcmap> -- a map extended with analysis information\n"
    << "from 'bloop' -- is provided, the program attempts to construct\n"
    << "the PROFILE by querying the <binary>'s debugging information.\n"
    << "Because of the better analysis ability of 'bloop', a <bloop-pcmap>\n"
    << "usually improves the accuracy of the PROFILE.  Moreover, because\n"
    << "no loop recovery is performed, providing <bloop-pcmap> enables\n"
    << "the PROFILE to represent loop nesting information.\n"
    << "[*Not fully implemented.*]\n"
    << "\n"
#endif     
    << "The following <profile> formats are currently supported: \n"
    << "  - HPCTools FIXME \n"
    << "\n"
    << "Options:\n"
    << "  -V: print version information\n"    
#if 0    
    << "  -m: specify <bloop-pcmap>\n"
#endif    
    
    << endl;
} 

Args::Args(int argc, char* const* argv)
{
  cmd = argv[0]; 

  bool printVersion = false;
  //other options: prettyPrintOutput = false;
  
  extern char *optarg;
  extern int optind;
  bool error = false;
  trace = 0;
  int c;
  while ((c = getopt(argc, argv, "Vm:d")) != EOF) {
    switch (c) {
    case 'm': {
      // A non-null value of 'pcMapFile' indicates it has been set
      if (optarg == NULL) { error = true; }
      pcMapFile = optarg;
      break; 
    }
    case 'V': { 
      printVersion = true;
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
  error = error || (optind != argc-2); 
  if (!error) {
    progFile = argv[optind];
    profFile = argv[optind+1]; 
  } 

  IFTRACE << "Args.cmd= " << cmd << endl; 
  IFTRACE << "Args.progFile= " << progFile << endl;
  IFTRACE << "Args.profFile= " << profFile << endl;
  IFTRACE << "Args.pcMapFile= " << pcMapFile << endl;
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

