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

static const char* usage_summary1 =
"--source [options] <profile-file>...";

static const char* usage_summary2 =
"--object [options] <profile-file>...";

static const char* usage_summary3 =
"--dump <profile-file>...\n";

static const char* usage_details = "\
hpcproftt correlates 'flat' profile metrics with either source code structure\n\
(first mode) or object code (second mode) and generates textual output\n\
suitable for a terminal.  In each of these modes, hpcproftt expects a list\n\
of flat profile files.\n\
\n\
hpcproftt also supports a third mode in which it generates textual dumps of\n\
profile files.  In this mode, the profile list may contain either flat or\n\
call path profile files.\n\
\n\
Options: General:\n\
  -v [<n>], --verbose [<n>]\n\
                       Verbose: generate progress messages to stderr at\n\
                       verbosity level <n>. {1}\n\
  -V, --version        Print version information.\n\
  -h, --help           Print this help.\n\
  --debug [<n>]        Debug: use debug level <n>. {1}\n\
\n\
Options: Source Structure Correlation:\n\
  --source=[fpls]      Defaults to showing all structure: load modules,\n\
  --src=[...]          files, procedures, loops, statements.\n\
                       Optionally select specific structure to show:\n\
                         f: files\n\
                         p: procedures\n\
                         l: loops\n\
                         s: statements\n\
  -I <path>, --include <path>\n\
                       Use <path> when searching for source files. May pass\n\
                       multiple times.\n\
  -S <file>, --structure <file>\n\
                       Use hpcstruct structure file <file> for correlation.\n\
                       May pass multiple times (e.g., for shared libraries).\n\
  --file=<path-substring>\n\
                       Annotate source files with path names that match\n\
                       <path-substring>.\n\
\n\
Options: Object Correlation:\n\
  --object[=s]         Show object code correlation: load modules, procedures\n\
  --obj[=s]            instructions.  Options:\n\
                         s: intermingle source line info with object code\n\
\n\
Options: Dump Raw Profile Data:\n\
  --dump               Generate textual representation of raw profile data.\n\
";


static bool isOptArg_obj(const char* x);


#define CLP CmdLineParser
#define CLP_SEPARATOR "***"

// Note: Changing the option name requires changing the name in Parse()
CmdLineParser::OptArgDesc Args::optArgs[] = {
  // Source structure correlation options
  {  0 , "source",          CLP::ARG_OPT , CLP::DUPOPT_CLOB, NULL,
     NULL },
  {  0 , "src",             CLP::ARG_OPT , CLP::DUPOPT_CLOB, NULL,
     NULL },
  { 'I', "include",         CLP::ARG_REQ,  CLP::DUPOPT_CAT,  CLP_SEPARATOR,
     NULL },
  { 'S', "structure",       CLP::ARG_REQ,  CLP::DUPOPT_CAT,  CLP_SEPARATOR,
     NULL },
  {  0 , "file",            CLP::ARG_REQ , CLP::DUPOPT_CAT,  CLP_SEPARATOR,
     NULL },

  // Object correlation options
  {  0 , "object",          CLP::ARG_OPT , CLP::DUPOPT_CLOB, NULL,
     isOptArg_obj },
  {  0 , "obj",             CLP::ARG_OPT , CLP::DUPOPT_CLOB, NULL,
     isOptArg_obj },

  // Raw profile data
  {  0 , "dump",            CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL,
     NULL },

  // General
  { 'v', "verbose",         CLP::ARG_OPT,  CLP::DUPOPT_CLOB, NULL,
     NULL },
  { 'V', "version",         CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL,
     NULL },
  { 'h', "help",            CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL,
     NULL },
  {  0 , "debug",           CLP::ARG_OPT,  CLP::DUPOPT_CLOB, NULL,  // hidden
     NULL },
  CmdLineParser_OptArgDesc_NULL_MACRO // SGI's compiler requires this version
};

#undef CLP


static bool 
isOptArg_obj(const char* x)
{
  return (x[0] == 's');
}



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
  mode = Mode_SourceCorrelation;
  Diagnostics_SetDiagnosticFilterLevel(1);

  // override Analysis::Args defaults
  db_dir = "";
  db_copySrcFiles = false;
  outFilename_XML = "";
  outFilename_TXT = "-";
  metrics_computeInteriorValues = true; // dump metrics on interior nodes

  // Object Correlation
  showSourceCode = false;
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
  os << "Usage: \n"
     << "  " << getCmd() << " " << usage_summary1 << endl
     << "  " << getCmd() << " " << usage_summary2 << endl
     << "  " << getCmd() << " " << usage_summary3 << endl
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
  // avoid error messages with: .../bin/hpcproftt-bin
  static string cmd = "hpcproftt";
  return cmd; // parser.getCmd(); 
}


void
Args::parse(int argc, const char* const argv[])
{
  try {
    // -------------------------------------------------------
    // Parse the command line
    // -------------------------------------------------------
    parser.parse(optArgs, argc, argv);
    
    // -------------------------------------------------------
    // Sift through results, checking for semantic errors
    // -------------------------------------------------------
    
    // Special options that should be checked first
    if (parser.isOpt("debug")) {
      int dbg = 1;
      if (parser.isOptArg("debug")) {
	const string& arg = parser.getOptArg("debug");
	dbg = (int)CmdLineParser::toLong(arg);
      }
      Diagnostics_SetDiagnosticFilterLevel(dbg);
      trace = dbg;
    }
    if (parser.isOpt("help")) { 
      printUsage(std::cerr); 
      exit(1);
    }
    if (parser.isOpt("version")) { 
      printVersion(std::cerr);
      exit(1);
    }
    if (parser.isOpt("verbose")) {
      int verb = 1;
      if (parser.isOptArg("verbose")) {
	const string& arg = parser.getOptArg("verbose");
	verb = (int)CmdLineParser::toLong(arg);
      }
      Diagnostics_SetDiagnosticFilterLevel(verb);
    }

    // Check for other options: Source correlation options
    if (parser.isOpt("source") || parser.isOpt("src")) {
      mode = Mode_SourceCorrelation;
      string opt;
      if (parser.isOptArg("source"))   { opt = parser.getOptArg("source"); }
      else if (parser.isOptArg("src")) { opt = parser.getOptArg("src"); }
          }
    if (parser.isOpt("include")) {
      string str = parser.getOptArg("include");
      StrUtil::tokenize(str, CLP_SEPARATOR, searchPaths);
      
      for (uint i = 0; i < searchPaths.size(); ++i) {
	searchPathTpls.push_back(Analysis::PathTuple(searchPaths[i], "src"));
      }
    }
    if (parser.isOpt("structure")) {
      string str = parser.getOptArg("structure");
      StrUtil::tokenize(str, CLP_SEPARATOR, structureFiles);
    }
    if (parser.isOpt("file")) {
      string str = parser.getOptArg("file");
      // ...
    }
    
    // Check for other options: Object correlation options
    if (parser.isOpt("object") || parser.isOpt("obj")) {
      mode = Mode_ObjectCorrelation;

      string opt;
      if (parser.isOptArg("object"))   { opt = parser.getOptArg("object"); }
      else if (parser.isOptArg("obj")) { opt = parser.getOptArg("obj"); }

      if (!opt.empty()) {
	if (opt == "s") {
	  showSourceCode = true;
	}
	else {
	  ARG_ERROR("Unknown argument to --obj,--object: '" << opt << "'");
	}
      }
    }

    // Check for other options: Dump raw profile data
    if (parser.isOpt("dump")) {
      mode = Mode_RawDataDump;
    }

    // FIXME: sanity check that options correspond to mode
    
    // Check for required arguments
    uint numArgs = parser.getNumArgs();
    if ( !(numArgs >= 1) ) {
      ARG_ERROR("Incorrect number of arguments!");
    }

    profileFiles.resize(numArgs);
    for (uint i = 0; i < numArgs; ++i) {
      profileFiles[i] = parser.getArg(i);
    }
  }
  catch (const CmdLineParser::ParseError& x) {
    ARG_ERROR(x.what());
  }
  catch (const CmdLineParser::Exception& x) {
    DIAG_EMsg(x.message());
    exit(1);
  }
}


void 
Args::dump(std::ostream& os) const
{
  os << "Args.cmd= " << getCmd() << endl; 
  Analysis::Args::dump(os);
}


