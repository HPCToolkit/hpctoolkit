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

//*************************** User Include Files ****************************

#include "Args.h"
#include <lib/support/Trace.h>

//*************************** Forward Declarations **************************

using std::cerr;
using std::endl;
using std::string;

//***************************************************************************

static const char* version_info =
#include <include/HPCToolkitVersionInfo.h>

static const char* usage_summary =
"[options] <binary>\n";

static const char* usage_details =
"bloop analyzes the binary or DSO <binary>, recovers information about\n"
"its source-line loop-nesting structure, and generates an XML scope tree (of\n"
"type PGM) to stdout.  It uses debugging information to gather source-line\n"
"data; see caveats below for common problems.\n"
"\n"
"Options:\n"
"  -v, --verbose        Verbose: generate progress messages to stderr\n"
"  -n, --normalize-off  Turn off scope tree normalization\n"
"  -i, --irreducible-interval-as-loop\n"
"                       Treat irreducible intervals as loops\n"
"  -c, --compact        Generate compact output, eliminating extra white\n"
"                       space\n"
"  -p <list>, --canonical-paths <list>\n"
"                       Ensure that scope tree only contains files found in\n"
"                       the colon-separated <list>. May be passed multiple\n"
"                       times.\n"
"  -V, --version        Print version information.\n"
"  -h, --help           Print this help.\n"
"  -D, --dump-binary    Dump binary information and suppress loop recovery\n"
"\n"
"Caveats:\n"
"* <binary> should be compiled with as much debugging info as possible (e.g.\n"
"  -g3 for some compilers). When using the Sun compiler, place debugging\n"
"  info _in_ the binary (-xs).\n"
"* Optimizing compilers may generate inaccurate debugging information.\n"
"  bloop cannot fix this.\n"
"* C++ mangling is compiler specific. bloop tries both the platform's and\n"
"  GNU's demangler, but if <binary> was produced with a proprietary compiler\n"
"  demangling will likely be unsuccessful. (Also, cross-platform usage.)\n";

#if 0 // FIXME: '[-m <pcmap>]'
"  -m: Create and write a [PC -> source-line] map to <pcmap>.\n"
"      The map is extended with loop analysis information and  \n"
"      can be used to improve the output of 'xProf'.\n"
"      [*Not fully implemented.*]\n"
#endif     


#define CLP CmdLineParser

// Note: Changing the option name requires changing the name in Parse()
CmdLineParser::OptArgDesc Args::optArgs[] = {

  // Options
  { 'v', "verbose",         CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL },
  { 'n', "normalize-off",   CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL },
  { 'i', "irreducible-interval-as-loop",
                            CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL },
  { 'c', "compact",         CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL },
  { 'p', "canonical-paths", CLP::ARG_REQ , CLP::DUPOPT_CAT,  ":" },
  {  0 , "pcmap",           CLP::ARG_REQ , CLP::DUPOPT_ERR,  NULL }, // hidden
  
  { 'V', "version",     CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL },
  { 'h', "help",        CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL },
  { 'D', "dump-binary", CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL },
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
  verboseMode = false;
  normalizeScopeTree = true;
  irreducibleIntervalIsLoop = false;
  prettyPrintOutput = true;
  dumpBinary = false;
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
    trace = 0;
    
    if (parser.IsOpt("debug")) { 
      trace = 1; 
      if (parser.IsOptArg("debug")) {
	const string& arg = parser.GetOptArg("debug");
	trace = (int)CmdLineParser::ToLong(arg);
      }
    }
    if (parser.IsOpt("help")) { 
      PrintUsage(std::cerr); 
      exit(1);
    }
    if (parser.IsOpt("version")) { 
      PrintVersion(std::cerr);
      exit(1);
    }
    
    // Check for other options
    if (parser.IsOpt("verbose")) { 
      verboseMode = true;
    } 
    if (parser.IsOpt("normalize-off")) { 
      normalizeScopeTree = false;
    } 
    if (parser.IsOpt("irreducible-interval-as-loop")) { 
      irreducibleIntervalIsLoop = true;
    } 
    if (parser.IsOpt("compact")) { 
      prettyPrintOutput = false;
    } 
    if (parser.IsOpt("canonical-paths")) { 
      canonicalPathList = parser.GetOptArg("canonical-paths");
    }
    if (parser.IsOpt("pcmap")) { 
      pcMapFile = parser.GetOptArg("pcmap");
    }
    if (parser.IsOpt("dump-binary")) { 
      dumpBinary = true;
    } 
    
    // Check for required arguments
    if (parser.GetNumArgs() != 1) {
      PrintError(std::cerr, "Incorrect number of arguments!");
      exit(1);
    }
    inputFile = parser.GetArg(0);
  }
  catch (CmdLineParser::ParseError& e) {
    PrintError(std::cerr, e.GetMessage());
    exit(1);
  }
}


void 
Args::Dump(std::ostream& os) const
{
  os << "Args.cmd= " << GetCmd() << endl; 
  os << "Args.dumpBinary= " << dumpBinary << endl;
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

