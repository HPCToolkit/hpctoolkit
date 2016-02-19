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
// Copyright ((c)) 2002-2016, Rice University
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

#include <cstring> // strlen

#include <dirent.h> 
#include <sys/types.h> 

//*************************** User Include Files ****************************

#include <include/hpctoolkit-config.h>

#include "Args.hpp"

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

const string Args::HPCTOOLKIT = "HPCTOOLKIT"; 

static const char* version_info = HPCTOOLKIT_VERSION_STRING;

static const char* usage_summary1 =
"[output-options] [correlation-options] <profile-file>...";

static const char* usage_summary2 =
"[output-options] --config <config-file>\n";

static const char* usage_details = "\
hpcprof-flat correlates flat profiling metrics with static source code\n\
structure and (by default) generates an Experiment database for use with\n\
hpcviewer. hpcprof-flat is invoked in one of two ways.  In the former,\n\
correlation options are specified on the command line along with a list of\n\
flat profile files.  In the latter, these options along with derived metrics\n\
are specified in the configuration file <config-file>.  Note that the first\n\
mode is generally sufficient since derived metrics may be computed in\n\
hpcviewer.  However, to facilitate the batch processing of the second mode,\n\
when run in the first mode, a sample configuration file (config.xml) is\n\
generated within the Experiment database.\n\
\n\
For optimal results, structure information from hpcstruct should be provided.\n\
Without structure information, hpcprof-flat will default to correlation using\n\
line map information.\n\
\n\
\n\
Options: General:\n\
  -v [<n>], --verbose [<n>]\n\
                       Verbose: generate progress messages to stderr at\n\
                       verbosity level <n>. {1}  (Use n=3 to debug path\n\
                       replacement if metric and program structure is not\n\
                       properly matched.)\n\
  -V, --version        Print version information.\n\
  -h, --help           Print help.\n\
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
  -R '<old-path>=<new-path>', --replace-path '<old-path>=<new-path>'\n\
                       Substitute instances of <old-path> with <new-path>;\n\
                       apply to all paths (profile's load map, source code)\n\
                       for which <old-path> is a prefix.  Use '\\' to escape\n\
                       instances of '=' within a path. May pass multiple\n\
                       times.\n\
\n\
Options: Output:\n\
  -o <db-path>, --db <db-path>, --output <db-path>\n\
                       Specify Experiment database name <db-path>.\n\
                       {./"Analysis_DB_DIR"}\n\
  --src [yes|no], --source [yes|no]\n\
                       Whether to copy source code files into Experiment\n\
                       database. {yes} By default, hpcprof-flat copies source\n\
                       files with performance metrics and that can be\n\
                       reached by PATH/REPLACE statements, resulting in a\n\
                       self-contained dataset that does not rely on an\n\
                       external source code repository.  Note that if\n\
                       copying is suppressed, the database is no longer\n\
                       self-contained.\n\
\n\
Output formats: Select different output formats and optionally specify the\n\
output filename <file> (located within the Experiment database). The output\n\
is sparse in the sense that it ignores program areas without profiling\n\
information. (Set <file> to '-' to write to stdout.)\n\
  -x [<file>], --experiment [<file>]\n\
                       Default. ExperimentXML format. {"Analysis_OUT_DB_EXPERIMENT"}\n\
                       NOTE: To disable, set <file> to 'no'.\n";

// FIXME: tallent: do we want this?
//--csv [<file>]       Comma-separated-value format. {"Analysis_OUT_DB_CSV"}\n
//                     Includes flat scope tree and loops. Useful for\n
//                     downstream external tools.\n";


#define CLP CmdLineParser
#define CLP_SEPARATOR "!!!"

// Note: Changing the option name requires changing the name in Parse()
CmdLineParser::OptArgDesc Args::optArgs[] = {
  // Config-file-mode
  {  0 , "config",          CLP::ARG_REQ,  CLP::DUPOPT_CLOB, NULL,
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
  { 'R', "replace-path",    CLP::ARG_REQ,  CLP::DUPOPT_CAT,  CLP_SEPARATOR,
     NULL},

  // Output options
  { 'o', "output",          CLP::ARG_REQ , CLP::DUPOPT_CLOB, NULL,
     NULL },
  {  0 , "db",              CLP::ARG_REQ , CLP::DUPOPT_CLOB, NULL,
     NULL },

  {  0 , "src",             CLP::ARG_OPT,  CLP::DUPOPT_CLOB, NULL,
     NULL },
  {  0 , "source",          CLP::ARG_OPT,  CLP::DUPOPT_CLOB, NULL,
     NULL },

  // Output formats
  { 'x', "experiment",      CLP::ARG_OPT,  CLP::DUPOPT_CLOB, NULL,
     NULL },
  {  0 , "csv",             CLP::ARG_OPT,  CLP::DUPOPT_CLOB, NULL,
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
  setHPCHome(); 
  Diagnostics_SetDiagnosticFilterLevel(1);

  configurationFileMode = false;
  
  // Analysis::ArgsA
  profflat_computeFinalMetricValues = true;
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
     << usage_details << endl;
} 


void 
Args::printError(std::ostream& os, const char* msg) const
{
  os << getCmd() << ": " << msg << endl
     << "Try '" << getCmd() << " --help' for more information." << endl;
}

void 
Args::printError(std::ostream& os, const std::string& msg) const
{
  printError(os, msg.c_str());
}


const std::string& 
Args::getCmd() const
{ 
  // avoid error messages with: .../bin/hpcprof-flat-bin
  static string cmd = "hpcprof-flat";
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

    // Check for Config-file-mode:
    if (parser.isOpt("config")) {
      configurationFile = parser.getOptArg("config");
    }
    configurationFileMode = (!configurationFile.empty());

    if (!configurationFileMode) {
      out_db_config = "config.xml"; // Analysis::Args
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
    
    if (parser.isOpt("replace-path")) {
      string arg = parser.getOptArg("replace-path");
      
      std::vector<std::string> replacePaths;
      StrUtil::tokenize_str(arg,CLP_SEPARATOR, replacePaths);
      
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

    // Check for other options: Output options
    if (parser.isOpt("output")) {
      db_dir = parser.getOptArg("output");
    }
    if (parser.isOpt("db")) {
      db_dir = parser.getOptArg("db");
    }

    if (parser.isOpt("source") || parser.isOpt("src")) {
      string opt;
      if (parser.isOptArg("source"))   { opt = parser.getOptArg("source"); }
      else if (parser.isOptArg("src")) { opt = parser.getOptArg("src"); }
      db_copySrcFiles = (opt != "no");
    }

    // Check for other options: Output formats
    if (parser.isOpt("experiment")) {
      out_db_experiment = Analysis_OUT_DB_EXPERIMENT;
      if (parser.isOptArg("experiment")) {
	out_db_experiment = parser.getOptArg("experiment");
      }
      if (out_db_experiment == "no") { // special case
	out_db_experiment = "";
      }
    }
    if (parser.isOpt("csv")) {
      out_db_csv = Analysis_OUT_DB_CSV;
      if (parser.isOptArg("csv")) {
	out_db_csv = parser.getOptArg("csv");
      }
      db_copySrcFiles = false;
    }
    
    // Check for required arguments
    uint numArgs = parser.getNumArgs();
    if (configurationFileMode) {
      if (numArgs != 0) {
	ARG_ERROR("Incorrect number of arguments!");
      }
    }
    else {
      if ( !(numArgs >= 1) ) {
	ARG_ERROR("Incorrect number of arguments!");
      }

      profileFiles.resize(numArgs);
      for (uint i = 0; i < numArgs; ++i) {
	profileFiles[i] = parser.getArg(i);
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
Args::dump(std::ostream& os) const
{
  os << "Args.cmd= " << getCmd() << endl; 
  os << "Args.hpcHome= " << hpcHome << endl; 
  os << "::trace " << ::trace << endl; 
  Analysis::Args::dump(os);
}


//***************************************************************************

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
