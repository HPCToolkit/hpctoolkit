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
"[options] <binary> <profile>\n";

static const char* usage_details =
"Correlates a CSPROF profile metrics with corresponding source code in order\n"
"to create an Experiment database (ExperimentXML format) for use with\n"
"hpcviewer.\n"
"\n"
"General options:\n"
"  -v, --verbose [<n>]  Verbose: generate progress messages to stderr at\n"
"                       verbosity level <n>.  [1]\n"
"  -V, --version        Print version information.\n"
"  -h, --help           Print this help.\n"
"\n"
"Correlation options:\n"
"  -I <path>, --include <path>\n"
"                       Use <path> when searching for source files. May pass\n"
"                       multiple times.\n"
"  -S <file>, --structure <file>\n"
"                       Use the bloop structure file <file> for correlation.\n"
"\n"
"Output options:\n"
"  -o <db-path>, --db <db-path>, --output <db-path>\n"
"                       Specify Experiment database name <db-path>.\n"
"                       [./"EXPERIMENTDB"]\n"
"                       Experiment format ["EXPERIMENTXML"]\n";


#define CLP CmdLineParser

// Note: Changing the option name requires changing the name in Parse()
CmdLineParser::OptArgDesc Args::optArgs[] = {
  // Output options
  { 'o', "output",          CLP::ARG_REQ , CLP::DUPOPT_CLOB, NULL },
  {  0 , "db",              CLP::ARG_REQ , CLP::DUPOPT_CLOB, NULL },

  { 'I', "include",         CLP::ARG_OPT,  CLP::DUPOPT_CAT,  ":"  },
  { 'S', "structure",       CLP::ARG_OPT,  CLP::DUPOPT_CLOB, NULL },

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


const std::string& 
Args::GetCmd() const
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
      PrintUsage(std::cerr); 
      exit(1);
    }
    if (parser.IsOpt("version")) { 
      PrintVersion(std::cerr);
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
      StrUtil::tokenize(str, ":", searchPaths);
    }
    if (parser.IsOpt("structure")) {
      structureFile = parser.GetOptArg("structure");
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
    if (parser.GetNumArgs() != 2) {
      PrintError(std::cerr, "Incorrect number of arguments!");
      exit(1);
    }
    progFile = parser.GetArg(0);
    profileFile = parser.GetArg(1);
  }
  catch (const CmdLineParser::ParseError& x) {
    PrintError(std::cerr, x.what());
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
  
  
  DIAG_Msg(2, "load module: " << progFile << "\n"
	   << "profile: " << profileFile << "\n"
	   << "output: " << dbDir);
  if (searchPaths.size() > 0) {
    DIAG_Msg(2, "search paths:");
    for (int i = 0; i < searchPaths.size(); ++i) {
      DIAG_Msg(2, "  " << searchPaths[i]);
    }
  }
}


void 
Args::Dump(std::ostream& os) const
{
  os << "Args.cmd= " << GetCmd() << endl; 
  os << "Args.dbDir= " << dbDir << endl; 
}

void 
Args::DDump() const
{
  Dump(std::cerr);
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
