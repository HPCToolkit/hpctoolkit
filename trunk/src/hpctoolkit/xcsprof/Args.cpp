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

#include <sys/stat.h> // for mkdir
#include <sys/types.h>
#include <sys/errno.h>

//*************************** User Include Files ****************************

#include "Args.hpp"
#include "CSProfileUtils.hpp"

#include <lib/support/diagnostics.h>
#include <lib/support/Trace.hpp>
#include <lib/support/StrUtil.hpp>

//*************************** Forward Declarations **************************

#define EXPERIMENTDB  "experiment-db"
#define EXPERIMENTXML "experiment.xml"

// FIXME
#ifndef xDEBUG
#define xDEBUG(flag, code) {if (flag) {code; fflush(stdout); fflush(stderr);}} 
#endif

#define DEB_PROCESS_ARGUMENTS 0

//***************************************************************************

static const char* version_info =
#include <include/HPCToolkitVersionInfo.h>

static const char* usage_summary =
"[options] <executable> <profile-file>...\n";

static const char* usage_details =
"xcsprof correlates dynamic call-path profiling metrics with static source\n"
"code structure and generates an Experiment database for use with hpcviewer.\n"
"It expects an executable <executable> and one or more associated call path\n"
"profiles.\n"
"\n"
"Options: General\n"
"  -v, --verbose [<n>]  Verbose: generate progress messages to stderr at\n"
"                       verbosity level <n>. {1}\n"
"  -V, --version        Print version information.\n"
"  -h, --help           Print this help.\n"
"  --debug [<n>]        Debug: use debug level <n>. {1}\n"
"\n"
"Options: Correlation\n"
"  -I <path>, --include <path>\n"
"                       Use <path> when searching for source files. May pass\n"
"                       multiple times.\n"
"  -S <file>, --structure <file>\n"
"                       Use the bloop structure file <file> for correlation.\n"
"                       May pass multiple times (e.g., for shared libraries).\n"
"\n"
"Options: Output\n"
"  -o <db-path>, --db <db-path>, --output <db-path>\n"
"                       Specify Experiment database name <db-path>.\n"
"                       {./"EXPERIMENTDB"}\n"
"                       Experiment format {"EXPERIMENTXML"}\n";


#define CLP CmdLineParser
#define CLP_SEPARATOR "***"

// Note: Changing the option name requires changing the name in Parse()
CmdLineParser::OptArgDesc Args::optArgs[] = {
  // Output options
  { 'o', "output",          CLP::ARG_REQ , CLP::DUPOPT_CLOB, NULL },
  {  0 , "db",              CLP::ARG_REQ , CLP::DUPOPT_CLOB, NULL },

  { 'I', "include",         CLP::ARG_OPT,  CLP::DUPOPT_CAT,  CLP_SEPARATOR },
  { 'S', "structure",       CLP::ARG_OPT,  CLP::DUPOPT_CAT,  CLP_SEPARATOR },

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
  Parse(argc, argv);
}


void
Args::Ctor()
{
  // arguments
  dbDir           = EXPERIMENTDB;
  OutFilename_XML = EXPERIMENTXML;

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
  // avoid error messages with: /.../HPCToolkit-x86_64-Linux/bin/xcsprof-bin
  static string cmd = "xcsprof";
  return cmd; // parser.GetCmd(); 
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

    // Check for other options:
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
      dbDir = parser.GetOptArg("output");
    }
    if (parser.IsOpt("db")) {
      dbDir = parser.GetOptArg("db");
    }
    dbDir = normalizeFilePath(dbDir);

    // Check for required arguments
    if ( !(parser.GetNumArgs() >= 2) ) {
      printError(std::cerr, "Incorrect number of arguments!");
      exit(1);
    }

    exeFile = parser.GetArg(0);
    for (int i = 1; i < parser.GetNumArgs(); ++i) {
      profileFiles.push_back(parser.GetArg(i));
    }
  }
  catch (const CmdLineParser::ParseError& x) {
    printError(std::cerr, x.what());
    exit(1);
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
  
  
  DIAG_Msg(2, "load module: " << exeFile << "\n"
	   << "profile[0]: " << profileFiles[0] << "\n"
	   << "output: " << dbDir);
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
  os << "Args.dbDir= " << dbDir << endl; 
}

void 
Args::ddump() const
{
  dump(std::cerr);
}


//***************************************************************************

void 
Args::createDatabaseDirectory() 
{
  bool uniqueDatabaseDirectoryCreated;

  if (mkdir(dbDir.c_str(), 
	    S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == -1) {
    if (errno == EEXIST) {
      // attempt to create dbDir+pid;
      pid_t myPid = getpid();
      string myPidStr = StrUtil::toStr(myPid);
      string dbDirPid = dbDir + "-" + myPidStr;
      DIAG_Msg(1, "Database '" << dbDir << "' already exists.  Trying " 
	       << dbDirPid);
      if (mkdir(dbDirPid.c_str(), 
		S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == -1) {
	DIAG_Die("Could not create alternate database directory " << dbDirPid);
      } 
      else {
	DIAG_Msg(1, "Created database directory: " << dbDirPid);
	dbDir = dbDirPid;
      }
    } 
    else {
      DIAG_Die("Could not create database directory " << dbDir);
    }
  }
}
