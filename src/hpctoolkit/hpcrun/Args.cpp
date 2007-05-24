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

// (Equivalent to DIAG_Die.) Based on Jean Utke's code in xaifBooster.
#define ARG_ERROR(streamArgs)                                        \
  { std::ostringstream WeIrDnAmE;                                    \
    WeIrDnAmE << streamArgs /*<< std::ends*/;                        \
    printError(std::cerr, WeIrDnAmE.str());                          \
    exit(1); }

//***************************************************************************

static const char* version_info =
#include <include/HPCToolkitVersionInfo.h>

static const char* usage_summary =
"[profiling-options] -- <command> [command-arguments]\n"
"[info-options]\n";

static const char* usage_details =
"hpcrun profiles the execution of an arbitrary command using statistical\n"
"sampling. During execution of <command>, the specified events will be\n"
"monitored.  Events may be specified using either PAPI platform independent\n"
"names or the native event names for the processor.  hpcrun can sample.\n"
"multiple events during a single execution.\n"
"\n"
"For each event 'e' and its period 'p', for every 'p' instances of 'e' a\n"
"counter associated with the instruction at the current program counter\n"
"location will be incremented.  When <command> terminates normally, a\n"
"profile -- a histogram of counts for instructions in each load module --\n"
"will be written to a file with the name\n"
"<command>.<event1>.<hostname>.<pid>.<tid>. If multiple events are\n"
"specified, data for all events will be recorded in this single file even\n"
"though the file naming convention only uses <event1> in the name of the\n"
"output file.\n"
"\n"
"The special option '--' can be used to stop hpcrun option parsing.  This is\n"
"especially useful when <command> takes arguments of its own.\n"
"\n"
"hpcrun allows the user to abort a process *and* write the partial profiling\n"
"data to disk by sending the Interrupt signal (INT or Ctrl-C).  This can be\n"
"extremely useful on long-running or misbehaving applications.\n"
"\n"
"Informational Options:\n"
"  -l, --events-short   List events available on this architecture \n"
"                       (system, PAPI, native)\n"
"  -L, --events-long    Similar to above but with more information.\n"
"  --paths              Print paths for external PAPI and MONITOR.\n"
"  -V, --version        Print version information.\n"
"  -h, --help           Print this help.\n"
"\n"
"Profiling Options: Defaults are shown in curly brackets {}.\n"
"  -r, --recursive [<yes|no>]                                          {no}\n"
"      By default all processes spawned by <command> will be profiled, each\n"
"      receiving its own output file. Use this option to turn off recursive\n"
"      profiling; only <command> will be profiled.\n"
"  -t, --threads <mode>                                              {each}\n"
"      Select thread profiling mode:\n"
"        each: Create separate profiles for each thread.\n"
"        all:  Create one combined profile of all threads.\n"
"      NOTE 1: The WALLCLK event cannot be used in a multithreaded process.\n"
"      NOTE 2: Only POSIX threads are supported.\n"
"  -e, --event <event>[:<period>]                      {PAPI_TOT_CYC:999999}\n"
"      An event to profile and its corresponding sample period.  <event>\n"
"      may be either a PAPI or native processor event\n"
"      NOTE 1: It is recommended that you always specify the sampling period\n"
"              for each profiling event.\n"
"      NOTE 2: You may use the special event WALLCLK to profile with wall\n"
"              clock time.  It may be used only *once* and cannot be used\n"
"              with another event.  It is an error to specify a period.\n"
"      NOTE 3: Multiple events may be selected for profiling during an\n" 
"              execution by using  multiple '-e' arguments. \n"
"      NOTE 4: The maximum number of events that can be monitored during a\n"
"              single execution depends on the processor. Not all combinations\n"
"              of events may be monitored in the same execution; allowable\n"
"              combinations depend on the processor. Check your\n"
"              processor documentation for the details of both issues.\n"
"  -o, --output <outpath>                                                {.}\n"
"      Directory for output data\n"
"  --papi-flag <flag>                                    {PAPI_POSIX_PROFIL}\n"
"      Profile style flag\n"
"\n"
"NOTES:\n"
"* Because hpcrun uses LD_PRELOAD to initiate profiling, it cannot be used\n"
"  to profile setuid commands.\n"
"* For the same reason, it cannot profile statically linked applications.\n"
"* Bug: For non-recursive profiling, LD_PRELOAD is currently unsetenv'd.\n"
"  Child processes that otherwise depend LD_PRELOAD will likely die.\n"
"\n";


#define CLP CmdLineParser

// Note: Changing the option name requires changing the name in Parse()
CmdLineParser::OptArgDesc Args::optArgs[] = {

  // Options: info
  { 'l', "events-short", CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL },
  { 'L', "events-long",  CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL },
  {  0 , "paths",        CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL },

  // Options: profiling
  { 'r', "recursive",   CLP::ARG_OPT,  CLP::DUPOPT_CLOB, NULL },
  { 't', "threads",     CLP::ARG_REQ,  CLP::DUPOPT_CLOB, NULL },
  { 'e', "event",       CLP::ARG_REQ,  CLP::DUPOPT_CAT,  ";"  },
  { 'o', "output",      CLP::ARG_REQ , CLP::DUPOPT_CLOB, NULL },
  { 'f', "papi-flag",   CLP::ARG_REQ , CLP::DUPOPT_CLOB, NULL },
  
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
  parse(argc, argv);
}

void
Args::Ctor()
{
  listEvents = LIST_NONE;
  printPaths = false;
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


void
Args::parse(int argc, const char* const argv[])
{
  try {
    bool requireCmd = true;

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
      printUsage(std::cerr); 
      exit(1);
    }
    if (parser.IsOpt("version")) { 
      printVersion(std::cerr);
      exit(1);
    }
     
    // Check for informational options
    if (parser.IsOpt("events-short")) { 
      listEvents = LIST_SHORT;
      requireCmd = false;
    } 
    if (parser.IsOpt("events-long")) { 
      listEvents = LIST_LONG;
      requireCmd = false;
    } 
    if (parser.IsOpt("paths")) { 
      printPaths = true;
      requireCmd = false;
    }

    // Check for profiling options    
    if (parser.IsOpt("recursive")) { 
      if (parser.IsOptArg("recursive")) {
	const string& arg = parser.GetOptArg("recursive");
	if (arg == "no" || arg == "yes") {
	  profRecursive = arg;
	}
	else {
	  ARG_ERROR("Unexpected option argument '" << arg << "'");
	}
      }
      else {
	profRecursive = "no";
      }
    }
    if (parser.IsOpt("threads")) { 
      const string& arg = parser.GetOptArg("threads");
      if (arg == "each" || arg == "all") {
	profThread = arg;
      }
      else {
	ARG_ERROR("Unexpected option argument '" << arg << "'");
      }
    }
    if (parser.IsOpt("event")) { 
      profEvents = parser.GetOptArg("event");
    }
    if (parser.IsOpt("output")) { 
      profOutput = parser.GetOptArg("output");
    }
    if (parser.IsOpt("papi-flag")) { 
      profPAPIFlag = parser.GetOptArg("papi-flag");
    }
    
    // Check for required arguments: Get <command> [command-arguments]
    uint numArgs = parser.GetNumArgs();
    if (requireCmd && numArgs < 1) {
      ARG_ERROR("Incorrect number of arguments: Missing <command> to profile.");
    }
    
    profArgV.resize(numArgs);
    for (uint i = 0; i < numArgs; ++i) {
      profArgV[i] = parser.GetArg(i);
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
}


void 
Args::ddump() const
{
  dump(std::cerr);
}


//***************************************************************************
