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

#include <lib/analysis/CallPath.hpp> /* for normalizeFilePath */

#include <lib/support/diagnostics.h>
#include <lib/support/Trace.hpp>
#include <lib/support/StrUtil.hpp>

//*************************** Forward Declarations **************************

// Cf. DIAG_Die.
#define ARG_ERROR(streamArgs)                                        \
  { std::ostringstream WeIrDnAmE;                                    \
    WeIrDnAmE << streamArgs /*<< std::ends*/;                        \
    printError(std::cerr, WeIrDnAmE.str());                          \
    exit(1); }

//***************************************************************************

static const char* version_info =
#include <include/HPCToolkitVersionInfo.h>

static const char* usage_summary =
"[options] <profile-file>...\n";

static const char* usage_details = "\
hpcprof correlates dynamic call path profiling metrics with static source\n\
code structure and generates an Experiment database for use with hpcviewer.\n\
It expects a list of call path profile files.\n\
\n\
Options: General:\n\
  -v, --verbose [<n>]  Verbose: generate progress messages to stderr at\n\
                       verbosity level <n>. {1}\n\
  -V, --version        Print version information.\n\
  -h, --help           Print this help.\n\
  --debug [<n>]        Debug: use debug level <n>. {1}\n\
\n\
Options: Source Structure Correlation:\n\
  -I <path>, --include <path>\n\
                       Use <path> when searching for source files. May pass\n\
                       multiple times.\n\
  -S <file>, --structure <file>\n\
                       Use hpcstruct structure file <file> for correlation.\n\
                       May pass multiple times (e.g., for shared libraries).\n\
\n\
Options: Output:\n\
  -o <db-path>, --db <db-path>, --output <db-path>\n\
                       Specify Experiment database name <db-path>.\n\
                       {./"Analysis_EXPERIMENTDB"}\n\
                       Experiment format {"Analysis_EXPERIMENTXML"}\n";


#define CLP CmdLineParser
#define CLP_SEPARATOR "***"

// Note: Changing the option name requires changing the name in Parse()
CmdLineParser::OptArgDesc Args::optArgs[] = {
  // Source structure correlation options
  { 'I', "include",         CLP::ARG_REQ,  CLP::DUPOPT_CAT,  CLP_SEPARATOR },
  { 'S', "structure",       CLP::ARG_REQ,  CLP::DUPOPT_CAT,  CLP_SEPARATOR },

  // Output options
  { 'o', "output",          CLP::ARG_REQ , CLP::DUPOPT_CLOB, NULL },
  {  0 , "db",              CLP::ARG_REQ , CLP::DUPOPT_CLOB, NULL },

  // General
  { 'v', "verbose",         CLP::ARG_OPT,  CLP::DUPOPT_CLOB, NULL },
  { 'V', "version",         CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL },
  { 'h', "help",            CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL },
  {  0 , "debug",           CLP::ARG_OPT,  CLP::DUPOPT_CLOB, NULL }, // hidden
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
  parse(argc, argv);
}


void
Args::Ctor()
{
  Diagnostics_SetDiagnosticFilterLevel(1);
}


Args::~Args()
{
}


void 
Args::printVersion(std::ostream& os) const
{
  os << getCmd() << ": " << version_info << endl;
}


void 
Args::printUsage(std::ostream& os) const
{
  os << "Usage: " << getCmd() << " " << usage_summary << endl
     << usage_details << endl;
} 


void 
Args::printError(std::ostream& os, const char* msg) const
{
  os << getCmd() << ": " << msg << endl
     << "Try `" << getCmd() << " --help' for more information." << endl;
}

void 
Args::printError(std::ostream& os, const std::string& msg) const
{
  printError(os, msg.c_str());
}


const std::string& 
Args::getCmd() const
{ 
  // avoid error messages with: .../bin/hpcprof-bin
  static string cmd = "hpcprof";
  return cmd; // parser.GetCmd(); 
}


void
Args::parse(int argc, const char* const argv[])
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
      trace = dbg;
    }
    if (parser.IsOpt("help")) { 
      printUsage(std::cerr); 
      exit(1);
    }
    if (parser.IsOpt("version")) { 
      printVersion(std::cerr);
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

    // Check for other options: Correlation options
    if (parser.IsOpt("include")) {
      string str = parser.GetOptArg("include");
      StrUtil::tokenize(str, CLP_SEPARATOR, searchPaths);
    }
    if (parser.IsOpt("structure")) {
      string str = parser.GetOptArg("structure");
      StrUtil::tokenize(str, CLP_SEPARATOR, structureFiles);
    }
    
    // Check for other options: Output options
    if (parser.IsOpt("output")) {
      db_dir = parser.GetOptArg("output");
    }
    if (parser.IsOpt("db")) {
      db_dir = parser.GetOptArg("db");
    }
    db_dir = normalizeFilePath(db_dir);

    // Check for required arguments
    uint numArgs = parser.GetNumArgs();
    if ( !(numArgs >= 1) ) {
      ARG_ERROR("Incorrect number of arguments!");
    }

    profileFiles.resize(numArgs);
    for (uint i = 0; i < numArgs; ++i) {
      profileFiles[i] = parser.GetArg(i);
    }
  }
  catch (const CmdLineParser::ParseError& x) {
    ARG_ERROR(x.what());
  }
  catch (const CmdLineParser::Exception& x) {
    DIAG_EMsg(x.message());
    exit(1);
  }

  // -------------------------------------------------------
  // Postprocess
  // -------------------------------------------------------

  char cwd[MAX_PATH_SIZE+1];
  getcwd(cwd, MAX_PATH_SIZE);

  for (std::vector<string>::iterator it = searchPaths.begin(); 
       it != searchPaths.end(); /* */) {
    string& x = *it; // current path
    std::vector<string>::iterator x_it = it;
    
    ++it; // advance iterator 
    
    if (chdir(x.c_str()) == 0) {
      char norm_x[MAX_PATH_SIZE+1];
      getcwd(norm_x, MAX_PATH_SIZE);
      x = norm_x; // replace x with norm_x
    }
    else {
      DIAG_Msg(1, "Discarding search path: " << x);
      searchPaths.erase(x_it);
    }
    chdir(cwd);
  }
  
  
  DIAG_Msg(2, "profile[0]: " << profileFiles[0] << "\n"
	   << "output: " << db_dir);
  if (searchPaths.size() > 0) {
    DIAG_Msg(2, "search paths:");
    for (int i = 0; i < searchPaths.size(); ++i) {
      DIAG_Msg(2, "  " << searchPaths[i]);
    }
  }
}


void 
Args::dump(std::ostream& os) const
{
  os << "Args.cmd= " << getCmd() << endl; 
  Analysis::Args::dump(os);
}


