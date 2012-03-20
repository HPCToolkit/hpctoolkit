// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2012, Rice University
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

#include <include/hpctoolkit-config.h>

#include "ArgsHPCProf.hpp"

#include "CallPath.hpp" /* for normalizeFilePath */

#include <lib/analysis/Util.hpp>

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

static const char* version_info = HPCTOOLKIT_VERSION_STRING;

static const char* usage_summary =
"[options] <measurement-group>...\n";

static const char* usage_details = "\
hpcprof and hpcprof-mpi analyze call path profile performance measurements,\n\
attribute them to static source code structure, and generate an Experiment\n\
database for use with hpcviewer. hpcprof-mpi is a scalable (parallel)\n\
version of hpcprof.\n\
\n\
Both hpcprof and hpcprof-mpi expect a list of measurement-groups, where a\n\
group is a call path profile directory or an individual profile file.\n\
\n\
N.B.: For best results (a) compile your application with debugging\n\
information (e.g., -g); (b) pass recursive search paths with the -I option;\n\
and (c) pass structure information with the -S option.\n\
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
  -M <metric>, --metric <metric>\n\
                       Specify the set of metrics computed by hpcprof.\n\
                       <metric> is one of the following:\n\
                         sum:    Show (only) summary metrics\n\
                                 (Sum, Mean, StdDev, CoefVar, Min, Max)\n\
                         sum+:   Show thread and summary metrics\n\
                         thread: Show only thread metrics\n\
                       Default is 'sum'; hpcprof-mpi always computes 'sum'.\n\
  -I <path>, --include <path>\n\
                       Use <path> when searching for source files. For a\n\
                       recursive search, append a + after the last slash,\n\
                       e.g., /mypath/+ . You use multiple -I options.\n\
  -S <file>, --structure <file>\n\
                       Use hpcstruct structure file <file> for correlation.\n\
                       May pass multiple times (e.g., for shared libraries).\n\
  -R '<old-path>=<new-path>', --replace-path '<old-path>=<new-path>'\n\
                       Substitute instances of <old-path> with <new-path>;\n\
                       apply to all paths (profile's load map, source code)\n\
                       for which <old-path> is a prefix.  Use '\\' to escape\n\
                       instances of '=' within a path. May pass multiple\n\
                       times.\n\
\n\
Options: Special:\n\
  --force-metric       Force hpcprof to show all thread-level metrics,\n\
                       regardless of their number.\n\
\n\
Options: Output:\n\
  -o <db-path>, --db <db-path>, --output <db-path>\n\
                       Specify Experiment database name <db-path>.\n\
                       {./"Analysis_DB_DIR"}\n\
                       Experiment format {"Analysis_OUT_DB_EXPERIMENT"}\n\
  --metric-db <yes|no>\n\
                       Control whether to generate a thread-level metric\n\
                       value database for hpcviewer scatter plots. {yes}\n\
";



#define CLP CmdLineParser
#define CLP_SEPARATOR "!!!"

// Note: Changing the option name requires changing the name in Parse()
CmdLineParser::OptArgDesc Analysis::ArgsHPCProf::optArgs[] = {
  {  0 , "agent-cilk",      CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL,
     NULL },
  {  0 , "agent-mpi",       CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL,
     NULL },
  {  0 , "agent-pthread",   CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL,
     NULL },

  // Source structure correlation options
  {  0 , "name",            CLP::ARG_REQ,  CLP::DUPOPT_CLOB, CLP_SEPARATOR,
     NULL },
  {  0 , "title",           CLP::ARG_REQ,  CLP::DUPOPT_CLOB, CLP_SEPARATOR,
     NULL },

  { 'M', "metric",          CLP::ARG_REQ,  CLP::DUPOPT_CLOB, NULL,
     NULL },

  { 'I', "include",         CLP::ARG_REQ,  CLP::DUPOPT_CAT,  CLP_SEPARATOR,
     NULL },
  { 'S', "structure",       CLP::ARG_REQ,  CLP::DUPOPT_CAT,  CLP_SEPARATOR,
     NULL },
  { 'R', "replace-path",    CLP::ARG_REQ,  CLP::DUPOPT_CAT,  CLP_SEPARATOR,
     NULL},

  { 'N', "normalize",       CLP::ARG_REQ,  CLP::DUPOPT_CLOB, NULL,
     NULL },

  // Special options
  {  0 , "force-metric",    CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL,
     NULL },

  // Output options
  { 'o', "output",          CLP::ARG_REQ , CLP::DUPOPT_CLOB, NULL,
     NULL },
  {  0 , "db",              CLP::ARG_REQ , CLP::DUPOPT_CLOB, NULL,
     NULL },
  {  0 , "metric-db",       CLP::ARG_REQ,  CLP::DUPOPT_CLOB, NULL,
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

  // Analysis::Args
  prof_metrics = Analysis::Args::MetricSet_SumOnly;

  db_makeMetricDB = true;
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

    // Check for agent options
    if (parser.isOpt("agent-cilk")) {
      if (!agent.empty()) { ARG_ERROR("Only one agent is supported!"); }
      agent = "agent-cilk";
    }
    if (parser.isOpt("agent-mpi")) {
      if (!agent.empty()) { ARG_ERROR("Only one agent is supported!"); }
      agent = "agent-mpi";
    }
    if (parser.isOpt("agent-pthread")) {
      if (!agent.empty()) { ARG_ERROR("Only one agent is supported!"); }
      agent = "agent-pthread";
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
    if (parser.isOpt("normalize")) { 
      const string& arg = parser.getOptArg("normalize");
      doNormalizeTy = parseArg_norm(arg, "--normalize/-N option");
    }

    if (parser.isOpt("replace-path")) {
      string arg = parser.getOptArg("replace-path");
      
      std::vector<std::string> replacePaths;
      StrUtil::tokenize_str(arg, CLP_SEPARATOR, replacePaths);
      
      for (uint i = 0; i < replacePaths.size(); ++i) {
	int occurancesOfEquals =
	  Analysis::Util::parseReplacePath(replacePaths[i]);
	
	if (occurancesOfEquals > 1) {
	  ARG_ERROR("Too many occurances of \'=\'; make sure to escape any \'=\' in your paths");
	}
	else if(occurancesOfEquals == 0) {
	  ARG_ERROR("The \'=\' between the old path and new path is missing");
	}
      }
    }

    if (parser.isOpt("metric")) {
      string opt = parser.getOptArg("metric");
      parseArg_metric(opt, "--metric/-M option");
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
    if (parser.isOpt("metric-db")) {
      const string& arg = parser.getOptArg("metric-db");
      db_makeMetricDB = CmdLineParser::parseArg_bool(arg, "--metric-db option");
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


    // For now, parse first file name to determine name of database
    if (!isDbDirSet) {
      std::string nm = makeDBDirName(profileFiles[0]);
      if (!nm.empty()) {
	db_dir = nm;
      }
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


//***************************************************************************

bool
ArgsHPCProf::parseArg_norm(const string& value, const char* errTag)
{
  if (value == "all") {
    return true;
  }
  else if (value == "none") {
    return false;
  }
  else {
    ARG_ERROR(errTag << ": Unexpected value received: '" << value << "'");
  }
}


// Cf. hpcproftt/Args::parseArg_metric()
void
ArgsHPCProf::parseArg_metric(const std::string& value, const char* errTag)
{
  if (value == "sum") {
    prof_metrics = Analysis::Args::MetricSet_SumOnly;
  }
  else if (value == "sum+") {
    // TODO: issue error with hpcprof-mpi
    prof_metrics = Analysis::Args::MetricSet_ThreadAndSum;
  }
  else if (value == "thread") {
    // TODO: issue error with hpcprof-mpi
    prof_metrics = Analysis::Args::MetricSet_ThreadOnly;
  }
  else {
    ARG_ERROR(errTag << ": Unexpected value received: '" << value << "'");
  }
}


std::string
ArgsHPCProf::makeDBDirName(const std::string& profileArg)
{
  static const string str1 = "hpctoolkit-";
  static const string str2 = "-measurements";

  string db_dir = "";
  
  // 'profileArg' has the following structure:
  //   <path>/[pfx]hpctoolkit-<nm>-measurements[sfx]/<file>.hpcrun

  const string& fnm = profileArg;
  size_t pos1 = fnm.find(str1);
  size_t pos2 = fnm.find(str2);
  if (pos1 < pos2 && pos2 != string::npos) {
    // ---------------------------------
    // prefix
    // ---------------------------------
    size_t pfx_a   = fnm.find_last_of('/', pos1);
    size_t pfx_beg = (pfx_a == string::npos) ? 0 : pfx_a + 1; // [inclusive
    size_t pfx_end = pos1;                                    // exclusive)
    string pfx = fnm.substr(pfx_beg, pfx_end - pfx_beg);

    // ---------------------------------
    // nm (N.B.: can have 'negative' length with fnm='hpctoolkit-measurements')
    // ---------------------------------
    size_t nm_beg = pos1 + str1.length();            // [inclusive
    size_t nm_end = (nm_beg > pos2) ? nm_beg : pos2; // exclusive)
    string nm = fnm.substr(nm_beg, nm_end - nm_beg);
    
    // ---------------------------------
    // suffix
    // ---------------------------------
    string sfx;
    size_t sfx_beg = pos2 + str2.length();            // [inclusive
    size_t sfx_end = fnm.find_first_of('/', sfx_beg); // exclusive)
    if (sfx_end == string::npos) {
      sfx_end = fnm.size();
    }
    if (sfx_beg < sfx_end) {
      sfx = fnm.substr(sfx_beg, sfx_end - sfx_beg);
    }
    
    db_dir = pfx + Analysis_DB_DIR_pfx;
    if (!nm.empty()) {
      db_dir += "-" + nm;
    }
    db_dir += "-" Analysis_DB_DIR_nm + sfx;
  }

  return db_dir;
}


//***************************************************************************

} // namespace Analysis
