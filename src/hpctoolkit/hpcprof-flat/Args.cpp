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

#include <dirent.h> 
#include <sys/types.h> 

//*************************** User Include Files ****************************

#include "Args.hpp"

#include <lib/support/diagnostics.h>
#include <lib/support/Trace.hpp>

//*************************** Forward Declarations **************************

// Cf. DIAG_Die.
#define ARG_ERROR(streamArgs)                                        \
  { std::ostringstream WeIrDnAmE;                                    \
    WeIrDnAmE << streamArgs /*<< std::ends*/;                        \
    printError(std::cerr, WeIrDnAmE.str());                          \
    exit(1); }

#define EXPERIMENTDB  "experiment-db"
#define EXPERIMENTXML "experiment.xml"
#define EXPERIMENTCSV "experiment.csv"
#define EXPERIMENTTSV "experiment.tsv"

//***************************************************************************

const string Args::HPCTOOLKIT = "HPCTOOLKIT"; 

static const char* version_info =
#include <include/HPCToolkitVersionInfo.h>

static const char* usage_summary1 =
"[output-options] [correlation-options] <profile-file>...";

static const char* usage_summary2 =
"[output-options] --config <config-file>\n";

static const char* usage_details = "\
hpcprof-flat correlates dynamic profiling metrics with static source code\n\
structure and (by default) generates an Experiment database for use with\n\
hpcviewer. hpcprof-flat is invoked in one of two ways.  In the former,\n\
correlation options are specified on the command line.  In the latter, these\n\
options along with derived metrics are specified in the configuration file.\n\
\n\
Options: General:\n\
  -v, --verbose [<n>]  Verbose: generate progress messages to stderr at\n\
                       verbosity level <n>. {1}  (Use n=2 to debug path\n\
                       replacement if metric and program structure is not\n\
                       properly matched.)\n\
  -V, --version        Print version information.\n\
  -h, --help           Print this help.\n\
  --debug [<n>]        Debug: use debug level <n>. {1}\n\
\n\
Options: Output:\n\
  -o <db-path>, --db <db-path>, --output <db-path>\n\
                       Specify Experiment database name <db-path>.\n\
                       {./"EXPERIMENTDB"}\n\
  --src [yes|no], --source [yes|no]\n\
                       Whether to copy source code files into Experiment\n\
                       database. {yes} By default, [hpcprof] copies source\n\
                       files with performance metrics and that can be\n\
                       reached by PATH/REPLACE statements, resulting in a\n\
                       self-contained dataset that does not rely on an\n\
                       external source code repository.  Note that if\n\
                       copying is suppressed, the database is no longer\n\
                       self-contained.\n\
\n\
Output formats: Select different output formats and optionally specify the\n\
output filename <fname> (located within the Experiment database). The output\n\
is sparse in the sense that it ignores program areas without profiling\n\
information. (Set <fname> to '-' to write to stdout.)\n\
  -x [<fname>], --experiment [<fname>]\n\
                       Default. ExperimentXML format. {"EXPERIMENTXML"}\n\
                       NOTE: To disable, set <fname> to 'no'.\n\
  --csv [<fname>]      Comma-separated-value format. {"EXPERIMENTCSV"}\n\
                       Includes flat scope tree and loops. Useful for\n\
                       downstream external tools.\n\
  --tsv [<fname>]      Tab-separated-value format. {"EXPERIMENTTSV"}\n\
                       Includes flat scope tree and lines. Useful for\n\
                       downstream external tools.\n";

#if 0
"Options: Correlation\n\
  <profile-files>      A list of hpcrun profiles. Requires bloop STRUCTURE\n\
                       information for correlation.\n\

  -l          By default, the generated scope tree contains aggregated\n\
              metrics at all internal nodes of the scope tree.  This option\n\
              saves space by outputting metrics only at the leaves. A\n\
              FUTURE version of HPCViewer will be able to use the option,\n\
              but no current software can.\n\
\n"
#endif


#define CLP CmdLineParser

// Note: Changing the option name requires changing the name in Parse()
CmdLineParser::OptArgDesc Args::optArgs[] = {
  // Output options
  { 'o', "output",          CLP::ARG_REQ , CLP::DUPOPT_CLOB, NULL },
  {  0 , "db",              CLP::ARG_REQ , CLP::DUPOPT_CLOB, NULL },

  {  0 , "src",             CLP::ARG_OPT,  CLP::DUPOPT_CLOB, NULL },
  {  0 , "source",          CLP::ARG_OPT,  CLP::DUPOPT_CLOB, NULL },

  // Output formats
  { 'x', "experiment",      CLP::ARG_OPT,  CLP::DUPOPT_CLOB, NULL },
  {  0 , "csv",             CLP::ARG_OPT,  CLP::DUPOPT_CLOB, NULL },
  {  0 , "tsv",             CLP::ARG_OPT,  CLP::DUPOPT_CLOB, NULL },

  // Config-file-mode
  {  0 , "config",          CLP::ARG_REQ,  CLP::DUPOPT_CLOB, NULL },

  { 'u', NULL,              CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL },

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
  setHPCHome(); 

  db_dir             = EXPERIMENTDB;
  outFilename_XML    = EXPERIMENTXML;
  outFilename_CSV    = "";
  outFilename_TSV    = "";
  db_copySrcFiles    = true;

  metrics_computeInteriorValues = true; // dump metrics on interior nodes

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
  os << "Usage: \n"
     << "  " << getCmd() << " " << usage_summary1 << endl
     << "  " << getCmd() << " " << usage_summary2 << endl
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
  // avoid error messages with: .../bin/hpcprof-flat-bin
  static string cmd = "hpcprof-flat";
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

    // Check for other options: Output options
    if (parser.IsOpt("output")) {
      db_dir = parser.GetOptArg("output");
    }
    if (parser.IsOpt("db")) {
      db_dir = parser.GetOptArg("db");
    }

    string cpysrc;
    if (parser.IsOpt("src")) {
      if (parser.IsOptArg("src")) { cpysrc = parser.GetOptArg("src"); }
    }
    if (parser.IsOpt("source")) {
      if (parser.IsOptArg("source")) { cpysrc = parser.GetOptArg("source"); }
    }
    if (!cpysrc.empty()) {
      db_copySrcFiles = (cpysrc != "no");
    }

    // Check for other options: Output formats
    if (parser.IsOpt("experiment")) {
      outFilename_XML = EXPERIMENTXML;
      if (parser.IsOptArg("experiment")) {
	outFilename_XML = parser.GetOptArg("experiment");
      }
    }
    if (parser.IsOpt("csv")) {
      outFilename_CSV = EXPERIMENTCSV;
      if (parser.IsOptArg("csv")) {
	outFilename_CSV = parser.GetOptArg("csv");
      }
      db_copySrcFiles = false; // FIXME:
    }
    if (parser.IsOpt("tsv")) { 
      outFilename_TSV = EXPERIMENTTSV;
      if (parser.IsOptArg("tsv")) {
	outFilename_TSV = parser.GetOptArg("tsv");
      }
      db_copySrcFiles = false; // FIXME:
    }
    
    // Check for other options:

    // Check for Config-file-mode:
    if (parser.IsOpt("config")) {
      configurationFile = parser.GetOptArg("config");
    }
    configurationFileMode = (!configurationFile.empty());
    
    // Check for required arguments
    uint numArgs = parser.GetNumArgs();
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
	profileFiles[i] = parser.GetArg(i);
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
  os << "Args.db_dir= " << db_dir << endl; 
  os << "Args.outFilename_XML= " << outFilename_XML << endl; 
  os << "Args.outFilename_CSV= " << outFilename_CSV << endl; 
  os << "Args.outFilename_TSV= " << outFilename_TSV << endl; 
  os << "Args.configurationFile= " << configurationFile << endl; 
  os << "::trace " << ::trace << endl; 
}

void 
Args::ddump() const
{
  dump(std::cerr);
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

