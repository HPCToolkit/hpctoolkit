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

#include "Args.hpp"

#include <lib/support/diagnostics.h>

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
"[profiling-options] -- <command> [command-arguments]";

static const char* usage_summary2 =
"[info-options]\n";

static const char* usage_details = "\
hpcrun profiles the execution of an arbitrary command <command> using\n\
statistical sampling.  It supports multiple sample sources during one\n\
execution and creates an IP (instruction pointer) histogram, or flat profile,\n\
for each sample source.  Specifically, for an event 'e' and period 'p', after\n\
every 'p' instances of 'e' a counter associated with the instruction of the\n\
current IP is incremented.  hpcrun profiles complex applications (forks,\n\
execs, threads and dynamically loaded libraries) and may be used in\n\
conjunction with parallel process launchers such as MPICH's mpiexec and\n\
SLURM's srun.\n\
\n\
When <command> terminates normally, a profile -- a histogram of counts for\n\
instructions in each load module -- will be written to a file with the name\n\
  <command>.hpcrun.<hostname>.<pid>.<tid>\n\
hpcrun allows the user to abort a process and write the partial profiling\n\
data to disk by sending the Interrupt signal (INT or Ctrl-C).  This can be\n\
extremely useful on long-running or misbehaving applications.\n\
\n\
The special option '--' can be used to stop hpcrun option parsing; this is\n\
especially useful when <command> takes arguments of its own.\n\
\n\
Options: Informational\n\
  -l, --events-short   List available events (NB: some may not be profilable)\n\
  -L, --events-long    Similar to above but with more information.\n\
  --paths              Print paths for external PAPI and MONITOR.\n\
  -V, --version        Print version information.\n\
  -h, --help           Print help.\n\
  --debug [<n>]        Debug: use debug level <n>. {1}\n\
\n\
Options: Profiling (Defaults shown in curly brackets {})\n\
  -r [<yes|no>], --recursive [<yes|no>]\n\
                       Profile processes spawned by <command>. {no} (Each\n\
                       process will receive its own output file.)\n\
  -t <mode>, --threads <mode>\n\
                       Select thread profiling mode. {each}\n\
                         each: Separate profiles for each thread.\n\
                         all:  Combined profiles of all threads.\n\
                       Only POSIX threads are supported; the WALLCLK event\n\
                       cannot be used in a multithreaded process.\n\
  -e <event>[:<period>], --event <event>[:<period>]\n\
                       An event to profile and its corresponding sample\n\
                       period. <event> may be either a PAPI or native\n\
                       processor event.  {PAPI_TOT_CYC:999999}. May pass\n\
                       multiple times.\n\
                       o Recommended: always specify sampling period.\n\
                       o Special event: WALLCLK (use once, without period)\n\
                       o Hardware and drivers limit possibilities.\n\
  -o <outpath>, --output <outpath>\n\
                       Directory for output data {.}\n\
  --papi-flag <flag>\n\
                       Profile style flag {PAPI_POSIX_PROFIL}\n\
\n\
NOTES:\n\
* Because hpcrun uses LD_PRELOAD to initiate profiling, it cannot be used\n\
  to profile setuid commands.\n\
* For the same reason, it cannot profile statically linked applications.\n\
* Bug: For non-recursive profiling, LD_PRELOAD is currently unsetenv'd.\n\
  Child processes that otherwise depend LD_PRELOAD will likely die.\n\
";


#define CLP CmdLineParser

// Note: Changing the option name requires changing the name in Parse()
CmdLineParser::OptArgDesc Args::optArgs[] = {
  // Options: info
  { 'l', "events-short", CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL, NULL },
  { 'L', "events-long",  CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL, NULL },
  {  0 , "paths",        CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL, NULL },

  // Options: profiling
  { 'r', "recursive",   CLP::ARG_OPT,  CLP::DUPOPT_CLOB, NULL, NULL },
  { 't', "threads",     CLP::ARG_REQ,  CLP::DUPOPT_CLOB, NULL, NULL },
  { 'e', "event",       CLP::ARG_REQ,  CLP::DUPOPT_CAT,  ";" , NULL },
  { 'o', "output",      CLP::ARG_REQ , CLP::DUPOPT_CLOB, NULL, NULL },
  { 'f', "papi-flag",   CLP::ARG_REQ , CLP::DUPOPT_CLOB, NULL, NULL },
  
  { 'V', "version",     CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL, NULL },
  { 'h', "help",        CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL, NULL },
  {  0 , "debug",       CLP::ARG_OPT,  CLP::DUPOPT_CLOB, NULL, NULL }, // hidden
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
  return parser.getCmd(); 
}


void
Args::parse(int argc, const char* const argv[])
{
  try {
    bool requireCmd = true;

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
    }
    if (parser.isOpt("help")) { 
      printUsage(std::cerr); 
      exit(1);
    }
    if (parser.isOpt("version")) { 
      printVersion(std::cerr);
      exit(1);
    }
     
    // Check for informational options
    if (parser.isOpt("events-short")) { 
      listEvents = LIST_SHORT;
      requireCmd = false;
    } 
    if (parser.isOpt("events-long")) { 
      listEvents = LIST_LONG;
      requireCmd = false;
    } 
    if (parser.isOpt("paths")) { 
      printPaths = true;
      requireCmd = false;
    }

    // Check for profiling options    
    if (parser.isOpt("recursive")) { 
      if (parser.isOptArg("recursive")) {
	const string& arg = parser.getOptArg("recursive");
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
    if (parser.isOpt("threads")) { 
      const string& arg = parser.getOptArg("threads");
      if (arg == "each" || arg == "all") {
	profThread = arg;
      }
      else {
	ARG_ERROR("Unexpected option argument '" << arg << "'");
      }
    }
    if (parser.isOpt("event")) { 
      profEvents = parser.getOptArg("event");
    }
    if (parser.isOpt("output")) { 
      profOutput = parser.getOptArg("output");
    }
    if (parser.isOpt("papi-flag")) { 
      profPAPIFlag = parser.getOptArg("papi-flag");
    }
    
    // Check for required arguments: get <command> [command-arguments]
    uint numArgs = parser.getNumArgs();
    if (requireCmd && numArgs < 1) {
      ARG_ERROR("Incorrect number of arguments: Missing <command> to profile.");
    }
    
    profArgV.resize(numArgs);
    for (uint i = 0; i < numArgs; ++i) {
      profArgV[i] = parser.getArg(i);
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
