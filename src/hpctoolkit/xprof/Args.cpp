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

static const char* version_info =
#include <include/HPCToolkitVersionInfo.h>

static const char* usage_summary1 =
"[-l | -L] <profile>\n";

static const char* usage_summary2 =
"[-V] [ [-M <mlist> -M...] [-X <xlist> -X...] [-R] ] [<binary>] <profile>\n";

static const char* usage_summary3 =
"Note: -p allows <profile> to be read from stdin.\n";

static const char* usage_details =
"Converts various types of profile output into the PROFILE format, which in\n"
"particular, associates source file line information from <binary> with\n"
"profile data from <profile>. In effect, the output is a [source-line to\n"
"PC-profile-data] map represented as a XML scope tree (e.g. file, procedure,\n"
"statement).  Output is sent to stdout.\n"
"\n"
"To find source line information, access to the profiled binary is required.\n"
"xprof will try to find the binary from data within <profile>.  If this\n"
"information is missing, <binary> must be explicitly specified.\n"
"\n"
"By default, xprof determines a set of metrics available for the given\n" 
"profile data and includes all of them in the PROFILE output.\n"
"\n"
"The following <profile> formats are currently supported: \n"
"  - DEC/Compaq/HP's DCPI 'dcpicat' (including ProfileMe) \n"
"\n"
"General options:\n"
"  -p       Supply <profile> on stdin.  E.g., it is often desirable to pipe\n"
"           the output of 'dcpicat' into xprof.\n"
"  -V       Print version information.\n"
"  -h       Print this help.\n"
"\n"    
"Listing available metrics:\n"
"  -l       List all derived metrics, in compact form, available from\n"
"           <profile> and suppress generation of PROFILE output.  Note that\n"
"           this output can be used with the -M option.\n"
"  -L       List all derived metrics, in long form, available from <profile>\n"
"           and suppress generation of PROFILE output.\n"
"\n"    
"Normal Mode:\n"
"  -M mlist Specify metrics to replace the default metric list.  Metrics in\n"
"           PROFILE output will follow this ordering. <mlist> is a colon-\n"
"           separated list; duplicates are allowed (though not recommended).\n"
"  -X xlist Specify metrics to exclude from the default metric list or from\n"
"           those specified with -M.  <xlist> is a colon-separated list.\n"
"  -R       (Most will not find this useful.) For some profile data, such as\n"
"           DCPI's ProfileMe, the default is to output derived metrics, not\n"
"           the underlying raw metrics; this option forces output of only\n"
"           the raw metrics.  Should not be used with -M or -X.\n";

#if 0  // FIXME '[-m <bloop-pcmap>]'
"If no <bloop-pcmap> -- a map extended with analysis information\n"
"from 'bloop' -- is provided, the program attempts to construct\n"
"the PROFILE by querying the <binary>'s debugging information.\n"
"Because of the better analysis ability of 'bloop', a <bloop-pcmap>\n"
"usually improves the accuracy of the PROFILE.  Moreover, because\n"
"no loop recovery is performed, providing <bloop-pcmap> enables\n"
"the PROFILE to represent loop nesting information.\n"
"[*Not fully implemented.*]\n"
"\n"
"  -m: specify <bloop-pcmap>\n"
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
  listAvailableMetrics = 0;
  outputRawMetrics = false;
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
  os << "Usage: " << endl
     << cmd << " " << usage_summary1
     << cmd << " " << usage_summary2
     << "  " << usage_summary3 << endl
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
  bool profFileFromStdin = false;
  
  // -------------------------------------------------------
  // Parse the command line
  // -------------------------------------------------------
  extern char *optarg;
  extern int optind;
  bool error = false;
  trace = 0;
  int c;
  while ((c = getopt(argc, (char**)argv, "Vhm:lLM:X:Rpd")) != EOF) {
    switch (c) {
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

    case 'p': { 
      profFileFromStdin = true;
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
  
  
  int argsleft = (argc - optind);
  
  // If we are to read the profile file from stdin, then there should
  // be at most one more argument.
  bool err = false;
  if (profFileFromStdin) {
    err = !(argsleft == 0 || argsleft == 1);
  } else {
    err = !(argsleft == 1 || argsleft == 2);
  }
  error |= err;

  // Sort out the program file and profile file
  if (!error) {
    if (profFileFromStdin) {
      if (argsleft == 1) {
	progFile = argv[optind];
      }
    } else {
      if (argsleft == 1) {
	profFile = argv[optind]; 
      } else {
	progFile = argv[optind];
	profFile = argv[optind+1]; 
      }
    }
  } 

  // Sanity check: -M,-X and -R should not be used at the same time
  if ( (!metricList.Empty() || !excludeMList.Empty()) && outputRawMetrics) {
    cerr << "Error: -M or -X cannot be used with -R.\n";
    error = true;
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
  os << "Args.pcMapFile= " << pcMapFile << endl;
  os << "Args.progFile= " << progFile << endl;
  os << "Args.profFile= " << profFile << endl;
  os << "::trace " << ::trace << endl; 
}

void 
Args::DDump() const
{
  Dump(std::cerr);
}
