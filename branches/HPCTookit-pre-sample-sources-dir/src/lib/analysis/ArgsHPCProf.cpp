// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2009, Rice University 
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
//   $HeadURL$
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

#include "ArgsHPCProf.hpp"

#include "CallPath.hpp" /* for normalizeFilePath */

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
"[options] <profile-dir-or-file>...\n";

static const char* usage_details = "\
hpcprof correlates call path profiling metrics with static source code\n\
structure and generates an Experiment database for use with hpcviewer. It\n\
expects a list of call path profile directories or files.\n\
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
  --name <name>, --title <name>\n\
                       Set the database's name (title) to <name>.\n\
  -I <path>, --include <path>\n\
                       Use <path> when searching for source files. For a\n\
                       recursive search, append a '*' after the last slash,\n\
                       e.g., '/mypath/*' (quote or escape to protect from\n\
                       the shell.) May pass multiple times.\n\
  -S <file>, --structure <file>\n\
                       Use hpcstruct structure file <file> for correlation.\n\
                       May pass multiple times (e.g., for shared libraries).\n\
\n\
Options: Special:\n\
  --force              Currently, hpcprof permits at most 32 profile-files\n\
                       to prevent unmanageably large Experiment databases.\n\
                       Use this option to remove this limit.  We are working\n\
                       on solutions.\n\
\n\
Options: Output:\n\
  -o <db-path>, --db <db-path>, --output <db-path>\n\
                       Specify Experiment database name <db-path>.\n\
                       {./"Analysis_DB_DIR"}\n\
                       Experiment format {"Analysis_OUT_DB_EXPERIMENT"}\n";



#define CLP CmdLineParser
#define CLP_SEPARATOR "!!!"

// Note: Changing the option name requires changing the name in Parse()
CmdLineParser::OptArgDesc Analysis::ArgsHPCProf::optArgs[] = {
  {  0 , "agent-pthread",   CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL,
     NULL },
  {  0 , "agent-cilk",      CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL,
     NULL },

  // Source structure correlation options
  {  0 , "name",            CLP::ARG_REQ,  CLP::DUPOPT_CLOB, CLP_SEPARATOR,
     NULL },
  {  0 , "title",           CLP::ARG_REQ,  CLP::DUPOPT_CLOB, CLP_SEPARATOR,
     NULL },

  { 'I', "include",         CLP::ARG_REQ,  CLP::DUPOPT_CAT,  CLP_SEPARATOR,
     NULL },
  { 'S', "structure",       CLP::ARG_REQ,  CLP::DUPOPT_CAT,  CLP_SEPARATOR,
     NULL },

  // Output options
  { 'o', "output",          CLP::ARG_REQ , CLP::DUPOPT_CLOB, NULL,
     NULL },
  {  0 , "db",              CLP::ARG_REQ , CLP::DUPOPT_CLOB, NULL,
     NULL },

  // Special options for now
  {  0 , "force",           CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL,
     NULL },

  // General
  { 'v', "verbose",         CLP::ARG_OPT,  CLP::DUPOPT_CLOB, NULL,
     CLP::isOptArg_long },
  { 'V', "version",         CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL,
     NULL },
  { 'h', "help",            CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL,
     NULL },
  {  0 , "debug",           CLP::ARG_OPT,  CLP::DUPOPT_CLOB, NULL,  // hidden
     CLP::isOptArg_long },
  CmdLineParser_OptArgDesc_NULL_MACRO // SGI's compiler requires this version
};

#undef CLP


//***************************************************************************
// ArgsHPCProf
//***************************************************************************

namespace Analysis {

ArgsHPCProf::ArgsHPCProf()
{
  Diagnostics_SetDiagnosticFilterLevel(1);
}


ArgsHPCProf::~ArgsHPCProf()
{
}


void 
ArgsHPCProf::printVersion(std::ostream& os) const
{
  os << getCmd() << ": " << version_info << endl;
}


void 
ArgsHPCProf::printUsage(std::ostream& os) const
{
  os << "Usage: " << getCmd() << " " << usage_summary << endl
     << usage_details << endl;
} 


void 
ArgsHPCProf::printError(std::ostream& os, const char* msg) const
{
  os << getCmd() << ": " << msg << endl
     << "Try '" << getCmd() << " --help' for more information." << endl;
}


void 
ArgsHPCProf::printError(std::ostream& os, const std::string& msg) const
{
  printError(os, msg.c_str());
}


void
ArgsHPCProf::parse(int argc, const char* const argv[])
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

    // Check for LUSH options (TODO)
    if (parser.isOpt("agent-pthread")) {
      lush_agent = "agent-pthread";
    }
    if (parser.isOpt("agent-cilk")) {
      lush_agent = "agent-cilk";
    }

    // Check for other options: Correlation options
    if (parser.isOpt("name")) {
      title = parser.getOptArg("name");
    }
    if (parser.isOpt("title")) {
      title = parser.getOptArg("title");
    }
    if (parser.isOpt("include")) {
      string str = parser.getOptArg("include");

      std::vector<std::string> searchPaths;
      StrUtil::tokenize_str(str, CLP_SEPARATOR, searchPaths);

      for (uint i = 0; i < searchPaths.size(); ++i) {
	searchPathTpls.push_back(Analysis::PathTuple(searchPaths[i], 
						     Analysis::DefaultPathTupleTarget));
      }
    }
    if (parser.isOpt("structure")) {
      string str = parser.getOptArg("structure");
      StrUtil::tokenize_str(str, CLP_SEPARATOR, structureFiles);
    }

    // Check for special hpcprof options:
    if (parser.isOpt("force")) {
      isHPCProfForce = true;
    }
    
    // Check for other options: Output options
    bool isDbDirSet = false;
    if (parser.isOpt("output")) {
      db_dir = parser.getOptArg("output");
      isDbDirSet = true;
    }
    if (parser.isOpt("db")) {
      db_dir = parser.getOptArg("db");
      isDbDirSet = true;
    }

    // Check for required arguments
    uint numArgs = parser.getNumArgs();
    if ( !(numArgs >= 1) ) {
      ARG_ERROR("Incorrect number of arguments!");
    }

    profileFiles.resize(numArgs);
    for (uint i = 0; i < numArgs; ++i) {
      profileFiles[i] = parser.getArg(i);
    }


    // TEMPORARY: parse first file name to determine name of database
    string mynm;
    if (!isDbDirSet) {
      // hpctoolkit-<nm>-measurements[-xxx]/<nm>-000000-tid-hostid-pid.hpcrun
      const string& fnm = profileFiles[0];
      size_t pos1 = fnm.find("hpctoolkit-");
      size_t pos2 = fnm.find("-measurements");
      if (pos1 == 0 && pos2 != string::npos) {
	size_t nm_beg = fnm.find_first_of('-') + 1; 
	size_t nm_end = pos2 - 1;
	mynm = fnm.substr(nm_beg, nm_end - nm_beg + 1);
	
	string id;
	size_t id_beg = fnm.find_first_of('-', pos2 + 1);
	size_t id_end = fnm.find_first_of('/', pos2 + 1);
	if (id_end != string::npos && id_beg < id_end) {
	  id = fnm.substr(id_beg, id_end - id_beg);
	}
	
	db_dir = Analysis_DB_DIR_pfx "-" + mynm + "-" Analysis_DB_DIR_nm + id;
      }
    }

    // TEMPORARY: 
    if (title.empty() && !mynm.empty()) {
      title = mynm;
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
ArgsHPCProf::dump(std::ostream& os) const
{
  os << "ArgsHPCProf.cmd= " << getCmd() << endl; 
  Analysis::Args::dump(os);
}

} // namespace Analysis
