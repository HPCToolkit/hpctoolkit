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
    << "  " << cmd << " [-l | -L]  <binary> <profile>\n"
    << "  " << cmd << " [-V] [ [-M <mlist> -M...] [-X <xlist> -X...] [-R] ] <binary> <profile>\n"
    
    << endl;
  cerr
    << "Converts various types of profile output into the PROFILE format,\n"
    << "which, in particular, associates source file line information from\n"
    << "<binary> with profile data from <profile>. In effect, the output is\n"
    << "a [source-line -> PC-profile-data] map represented as a XML scope\n"
    << "tree (e.g. file, procedure, statement).  Output is sent to stdout.\n"
    << "\n"
    << "By default, xprof determines a set of metrics available for the\n" 
    << "given profile data and includes all of them in the PROFILE output.\n"
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
    << "  -m: specify <bloop-pcmap>\n"
#endif     
    << "The following <profile> formats are currently supported: \n"
    << "  - DEC/Compaq/HP's DCPI 'dcpicat' (including ProfileMe) \n"
    << "\n"
    << "Listing available metrics:\n"
    << "  -l       List all derived metrics, in compact form, available from\n"
    << "           <profile> and suppress generation of PROFILE output.\n"
    << "           Note that this output can be used with the -M option.\n"
    << "  -L       List all derived metrics, in long form, available from\n"
    << "           <profile> and suppress generation of PROFILE output.\n"
    << "\n"    
    << "Normal Mode:\n"
    << "  -V       Print version information.\n"
    << "  -M mlist Specify metrics to replace the default metric list.\n"
    << "           Metrics in PROFILE output will follow this ordering.\n"
    << "           <mlist> is a colon-separated list; duplicates are allowed\n"
    << "           (though not recommended).\n"
    << "  -X xlist Specify metrics to exclude from the default metric list\n"
    << "           or from those specified with -M.  <xlist> is a colon-\n"
    << "           separated list.\n"
    << "  -R       (Most will not find this useful.) For some profile data,\n"
    << "           such as DCPI's ProfileMe, the default is to output derived\n"
    << "           metrics, not the underlying raw metrics; this option\n"
    << "           forces output of only the raw metrics.  Should not be\n"
    << "           used with -M or -X.\n"
    << endl;
} 

Args::Args(int argc, char* const* argv)
{
  cmd = argv[0]; 

  bool printVersion = false;
  listAvailableMetrics = 0; // 0: no, 1: short, 2: long
  outputRawMetrics = false;
  
  extern char *optarg;
  extern int optind;
  bool error = false;
  trace = 0;
  int c;
  while ((c = getopt(argc, argv, "Vm:lLM:X:Rd")) != EOF) {
    switch (c) {
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

    case 'l': { 
      listAvailableMetrics = 1;
      break; 
    }
    case 'L': { 
      listAvailableMetrics = 2;
      break; 
    }

    case 'M': { // may occur multiple times
      if (optarg == NULL) { error = true; }
      if (!metricList.Empty()) { metricList += ":"; }
      metricList += optarg;
      break; 
    }
    case 'X': { // may occur multiple times
      if (optarg == NULL) { error = true; }
      if (!excludeMList.Empty()) { excludeMList += ":"; }
      excludeMList += optarg;
      break; 
    }
    case 'R': { 
      outputRawMetrics = true;
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

  // Sanity check: -M,-X and -R should not be used at the same time
  if ( (!metricList.Empty() || !excludeMList.Empty()) && outputRawMetrics) {
    cerr << "Error: -M or -X cannot be used with -R.\n";
    error = true;
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

