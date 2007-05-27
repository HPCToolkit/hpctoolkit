// -*-Mode: C++;-*-
// $Id$

// * BeginRiceCopyright *****************************************************
// 
// Copyright ((c)) 2002-2007, Rice University 
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
//   $Source$
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

//************************* System Include Files ****************************

#include <iostream>
using std::cerr;
using std::endl;

#include <string>
using std::string;

//*************************** User Include Files ****************************

#include "Args.hpp"

#include <lib/support/diagnostics.h>

//*************************** Forward Declarations **************************

//***************************************************************************

static const char* version_info =
#include <include/HPCToolkitVersionInfo.h>

static const char* usage_summary =
"[options] <binary>\n";

static const char* usage_details =
"Given an application binary or DSO <binary>, bloop recovers the program\n"
"structure of its object code and writes to standard output a program\n"
"structure to object code mapping. bloop is designed primarily for highly\n"
"optimized binaries created from C, C++ and Fortran source code. Because\n"
"bloop's algorithms exploit a binary's debugging information, for best\n"
"results, binary should be compiled with standard debugging information.\n"
"bloop's output is typically passed to an HPCToolkit's correlation tool.\n"
"See the documentation for more information."
"\n"
"Options: General\n"
"  -v, --verbose [<n>]  Verbose: generate progress messages to stderr at\n"
"                       verbosity level <n>. {1}\n"
"  -V, --version        Print version information.\n"
"  -h, --help           Print this help.\n"
"  --debug [<n>]        Debug: use debug level <n>. {1}\n"
"\n"
"Options: Recovery and Output\n"
"  -i, --irreducible-interval-as-loop\n"
"                       Treat irreducible intervals as loops\n"
"  -p <list>, --canonical-paths <list>\n"
"                       Ensure that scope tree only contains files found in\n"
"                       the colon-separated <list>. May be passed multiple\n"
"                       times.\n"
"  -n, --normalize-off  Turn off scope tree normalization\n"
"  -u, --unsafe-normalize-off\n"
"                       Turn off potentially unsafe normalization\n"
"  -c, --compact        Generate compact output, eliminating extra white\n"
"                       space\n";


#define CLP CmdLineParser

// Note: Changing the option name requires changing the name in Parse()
CmdLineParser::OptArgDesc Args::optArgs[] = {

  // Options
  { 'i', "irreducible-interval-as-loop",
                            CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL },
  { 'p', "canonical-paths", CLP::ARG_REQ , CLP::DUPOPT_CAT,  ":" },

  { 'n', "normalize-off",   CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL },
  { 'u', "unsafe-normalize-off", 
                            CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL },
  { 'c', "compact",         CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL },
  
  // Options
  { 'v', "verbose",     CLP::ARG_OPT,  CLP::DUPOPT_CLOB, NULL },
  { 'V', "version",     CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL },
  { 'h', "help",        CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL },
  {  0 , "debug",       CLP::ARG_OPT,  CLP::DUPOPT_CLOB, NULL }, // hidden
  CmdLineParser_OptArgDesc_NULL_MACRO // SGI's compiler requires this version
};

#undef CLP


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
  normalizeScopeTree = true;
  unsafeNormalizations = true;
  irreducibleIntervalIsLoop = false;
  prettyPrintOutput = true;
}


Args::~Args()
{
}


void 
Args::PrintVersion(std::ostream& os) const
{
  os << GetCmd() << ": " << version_info << endl;
}


void 
Args::PrintUsage(std::ostream& os) const
{
  os << "Usage: " << GetCmd() << " " << usage_summary << endl
     << usage_details << endl;
} 


void 
Args::PrintError(std::ostream& os, const char* msg) const
{
  os << GetCmd() << ": " << msg << endl
     << "Try `" << GetCmd() << " --help' for more information." << endl;
}

void 
Args::PrintError(std::ostream& os, const std::string& msg) const
{
  PrintError(os, msg.c_str());
}


void
Args::Parse(int argc, const char* const argv[])
{
  try {
    // -------------------------------------------------------
    // Parse the command line
    // -------------------------------------------------------
    parser.Parse(optArgs, argc, argv);
    
    // -------------------------------------------------------
    // Sift through results, checking for semantic errors
    // -------------------------------------------------------
    
    // Special options that should be checked first
    if (parser.IsOpt("debug")) { 
      int dbg = 1;
      if (parser.IsOptArg("debug")) {
	const string& arg = parser.GetOptArg("debug");
	dbg = (int)CmdLineParser::ToLong(arg);
      }
      Diagnostics_SetDiagnosticFilterLevel(dbg);
    }
    if (parser.IsOpt("help")) { 
      PrintUsage(std::cerr); 
      exit(1);
    }
    if (parser.IsOpt("version")) { 
      PrintVersion(std::cerr);
      exit(1);
    }
    if (parser.IsOpt("verbose")) {
      int verb = 1;
      if (parser.IsOptArg("verbose")) {
	const string& arg = parser.GetOptArg("verbose");
	verb = (int)CmdLineParser::ToLong(arg);
      }
      Diagnostics_SetDiagnosticFilterLevel(verb);
    } 
    
    // Check for other options
    if (parser.IsOpt("irreducible-interval-as-loop")) { 
      irreducibleIntervalIsLoop = true;
    } 
    if (parser.IsOpt("canonical-paths")) { 
      canonicalPathList = parser.GetOptArg("canonical-paths");
    }
    if (parser.IsOpt("normalize-off")) { 
      normalizeScopeTree = false;
    } 
    if (parser.IsOpt("unsafe-normalize-off")) { 
      unsafeNormalizations = false;
    } 
    if (parser.IsOpt("compact")) { 
      prettyPrintOutput = false;
    } 
    
    // Check for required arguments
    if (parser.GetNumArgs() != 1) {
      PrintError(std::cerr, "Incorrect number of arguments!");
      exit(1);
    }
    inputFile = parser.GetArg(0);
  }
  catch (const CmdLineParser::ParseError& x) {
    PrintError(std::cerr, x.what());
    exit(1);
  }
  catch (const CmdLineParser::Exception& x) {
    DIAG_EMsg(x.message());
    exit(1);
  }
}


void 
Args::Dump(std::ostream& os) const
{
  os << "Args.cmd= " << GetCmd() << endl; 
  os << "Args.prettyPrintOutput= " << prettyPrintOutput << endl;
  os << "Args.normalizeScopeTree= " << normalizeScopeTree << endl;
  os << "Args.canonicalPathList= " << canonicalPathList << endl;
  os << "Args.inputFile= " << inputFile << endl;
}

void 
Args::DDump() const
{
  Dump(std::cerr);
}


//***************************************************************************

#if 0
void 
Args::setHPCHome() 
{
  char * home = getenv(HPCTOOLKIT.c_str()); 
  if (home == NULL) {
    cerr << "Error: Please set your " << HPCTOOLKIT << " environment variable."
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
  hpcHome = home; 
} 
#endif  

