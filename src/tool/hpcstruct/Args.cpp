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
// Copyright ((c)) 2002-2021, Rice University
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

#include "Args.hpp"

#include <lib/analysis/Util.hpp>
#include <lib/support/diagnostics.h>
#include <lib/support/FileUtil.hpp>
#include <lib/support/StrUtil.hpp>

//*************************** Forward Declarations **************************

// Cf. DIAG_Die.
#define ARG_ERROR(streamArgs)                                        \
  { std::ostringstream WeIrDnAmE;                                    \
    WeIrDnAmE << streamArgs /*<< std::ends*/;                        \
    printError(std::cerr, WeIrDnAmE.str());                          \
    exit(1); }

//***************************************************************************

// Size in bytes for parallel analysis of gpu binaries
#define DEFAULT_GPU_SIZE   100000000
#define GPU_SIZE_STR      "100000000"

static const char* version_info = HPCTOOLKIT_VERSION_STRING;

static const char* usage_summary =
"hpcstruct [options] <binary>\n\
   or: hpcstruct [options] <measurement directory for GPU-accelerated application>\n";

static const char* usage_details = "\
Given an application binary, a shared library, or a GPU binary, hpcstruct\n\
recovers the program structure of its object code.  Program structure is a\n\
mapping of a program's object code to its static source-level structure.\n\
By default, hpcstruct writes its results to the file 'basename(<binary>).hpcstruct'.\n\
To improve attribution of performance measurements to program source code, one can\n\
pass one or more program structure files to HPCToolkit's analysis tool hpcprof\n\
along with one or more HPCToolkit performance measurement directories.\n\
\n\
During execution of a GPU-accelerated application, HPCToolkit records\n\
GPU binaries in the application's measurement directory. To attribute\n\
performance to GPU functions in a GPU-accelerated application, one should \n\
apply hpcstruct to the application's HPCToolkit measurement directory to\n\
analyze all GPU binaries recorded within. When analyzing a measurement directory\n\
that includes GPU binaries, any program structure files produced will be recorded\n\
inside the measurement directory. When hpcprof is applied to a measurement\n\
directory that contains program structure files for GPU binaries, these program\n\
structure files will be used to help attribute any GPU performance measurements.\n\
\n\
hpcstruct is designed primarily for highly optimized binaries created from\n\
C, C++, Fortran, CUDA, and HIP source code. Because hpcstruct's algorithms exploit a\n\
binary's debugging information, for best results, binary should be compiled\n\
with standard debugging information or at a minimum, line map information.\n\
See the HPCToolkit manual for more information.\n\
\n\
For faster analysis of large binaries or many GPU binaries, we recommend using\n\
the -j option to employ multithreading. As many as 32 cores can be used profitably\n\
to analyze large CPU or GPU binaries in the measurements directory for a\n\
GPU-accelerated application.\n\
\n\
Options: General\n\
  -V, --version        Print version information.\n\
  -h, --help           Print this help message.\n\
  --debug=[<n>]        Debug: use debug level <n>. {1}\n\
  -v [<n>], --verbose [<n>]  Verbose: generate progress messages to stderr\n\
                       at verbosity level <n>. {1}\n\
\n\
Options: Parallel usage\n\
  -j <num>, --jobs <num>  Use <num> threads for all phases in hpcstruct. {1}\n\
  --gpu-size <n>       Size (bytes) of a GPU binary that will cause hpcstruct\n\
                       to use <num> threads to analyze a binary in parallel.\n\
                       GPU binaries with fewer than <n> bytes will be analyzed\n\
                       concurrently, <num> at a time.  {" GPU_SIZE_STR "}\n\
\n\
Options: Structure recovery\n\
  --gpucfg <yes/no>    Compute loop nesting structure for GPU machine code.\n\
                       Loop nesting structure is only useful with\n\
                       instruction-level measurements collected using PC\n\
                       sampling or instrumentation. {no} \n\
  -I <path>, --include <path>\n\
                       Use <path> when resolving source file names. For a\n\
                       recursive search, append a '*' after the last slash,\n\
                       e.g., '/mypath/*' (quote or escape to protect from\n\
                       the shell.) May pass multiple times.\n\
  -R '<old-path>=<new-path>', --replace-path '<old-path>=<new-path>'\n\
                       Substitute instances of <old-path> with <new-path>;\n\
                       apply to all paths (profile's load map, source code)\n\
                       for which <old-path> is a prefix.  Use '\\' to escape\n\
                       instances of '=' within a path. May pass multiple\n\
                       times.\n\
\n\
Options: Output files\n\
  -o <file>, --output <file>\n\
                       Write hpcstruct file to <file>.\n\
                       Use '--output=-' to write output to stdout.\n\
  --du-graph <yes/no>  Creates def-use graphs from gpubins.\n\
                       Currently supported only for intel gpubins\n\
                       output format is ascii at present. {no} \n\
\n\
Options for Developers:\n\
  --jobs-struct <num>  Use <num> threads for the MakeStructure() phase only.\n\
  --jobs-parse  <num>  Use <num> threads for the ParseAPI::parse() phase only.\n\
  --jobs-symtab <num>  Use <num> threads for the Symtab phase (if possible).\n\
  --show-gaps          Feature to show unclaimed vma ranges (gaps)\n\
                       in the control-flow graph.\n\
  --time               Display stats on hpcstruct's time and space usage.\n\
";

#define CLP CmdLineParser
#define CLP_SEPARATOR "!!!"

// Note: Changing the option name requires changing the name in Parse()
CmdLineParser::OptArgDesc Args::optArgs[] = {
  { 'j',  "jobs",  CLP::ARG_REQ,  CLP::DUPOPT_CLOB,  NULL,  NULL },
  {  0 ,  "jobs-struct",  CLP::ARG_REQ,  CLP::DUPOPT_CLOB,  NULL,  NULL },
  {  0 ,  "jobs-parse",   CLP::ARG_REQ,  CLP::DUPOPT_CLOB,  NULL,  NULL },
  {  0 ,  "jobs-symtab",  CLP::ARG_REQ,  CLP::DUPOPT_CLOB,  NULL,  NULL },
  {  0 ,  "gpu-size",     CLP::ARG_REQ,  CLP::DUPOPT_CLOB,  NULL,  NULL },
  {  0 ,  "time",         CLP::ARG_NONE, CLP::DUPOPT_CLOB,  NULL,  NULL },

  // Structure recovery options
  {  0 ,  "gpucfg",         CLP::ARG_REQ,  CLP::DUPOPT_CLOB,  NULL,  NULL },
  { 'I', "include",         CLP::ARG_REQ,  CLP::DUPOPT_CAT,  ":",
     NULL },
  { 'R', "replace-path",    CLP::ARG_REQ,  CLP::DUPOPT_CAT,  CLP_SEPARATOR,
     NULL},
  {  0 , "show-gaps",       CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL,
     NULL },

  // Output options
  { 'o', "output",          CLP::ARG_REQ , CLP::DUPOPT_CLOB, NULL,
     NULL },
  { '0', "du-graph",        CLP::ARG_REQ , CLP::DUPOPT_CLOB, NULL, NULL },

  // General
  { 'v', "verbose",     CLP::ARG_OPT,  CLP::DUPOPT_CLOB, NULL,
     CLP::isOptArg_long },
  { 'V', "version",     CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL,
     NULL },
  { 'h', "help",        CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL,
     NULL },
  {  0 , "debug",       CLP::ARG_OPT,  CLP::DUPOPT_CLOB, NULL,
     CLP::isOptArg_long },
  {  0 , "debug-proc",  CLP::ARG_REQ,  CLP::DUPOPT_CLOB, NULL,
     NULL },

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
  jobs = -1;
  jobs_struct = -1;
  jobs_parse = -1;
  jobs_symtab = -1;
  show_time = false;
  gpu_size = DEFAULT_GPU_SIZE;
  searchPathStr = ".";
  show_gaps = false;
  compute_gpu_cfg = false;
  du_graph = false;
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
  os << "Usage: " << usage_summary << endl
     << usage_details << endl;
}


void
Args::printError(std::ostream& os, const char* msg) const
{
  os << "ERROR: " << msg << endl
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
  static const std::string command = std::string("hpcstruct");
  return command;
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
    }
    if (parser.isOpt("help")) {
      printUsage(std::cout);
      exit(0);
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
    if (parser.isOpt("debug-proc")) {
      dbgProcGlob = parser.getOptArg("debug-proc");
    }

    // Number of openmp threads (jobs, jobs-parse)
    if (parser.isOpt("jobs")) {
      const string & arg = parser.getOptArg("jobs");
      jobs = (int) CmdLineParser::toLong(arg);
    }
    if (parser.isOpt("jobs-struct")) {
      const string & arg = parser.getOptArg("jobs-struct");
      jobs_struct = (int) CmdLineParser::toLong(arg);
    }
    if (parser.isOpt("jobs-parse")) {
      const string & arg = parser.getOptArg("jobs-parse");
      jobs_parse = (int) CmdLineParser::toLong(arg);
    }
    if (parser.isOpt("jobs-symtab")) {
      const string & arg = parser.getOptArg("jobs-symtab");
      jobs_symtab = (int) CmdLineParser::toLong(arg);
    }
    if (parser.isOpt("gpu-size")) {
      const string & arg = parser.getOptArg("gpu-size");
      gpu_size = CmdLineParser::toLong(arg);
    }
    if (parser.isOpt("gpucfg")) {
      const string & arg = parser.getOptArg("gpucfg");
      bool yes = strcasecmp("yes", arg.c_str()) == 0;
      bool no = strcasecmp("no", arg.c_str()) == 0;
      if (!yes && !no) ARG_ERROR("gpucfg argument must be 'yes' or 'no'.");
      compute_gpu_cfg = yes;
    }
    if (parser.isOpt("du-graph")) {
      const string & arg = parser.getOptArg("du-graph");
      bool yes = strcasecmp("yes", arg.c_str()) == 0;
      bool no = strcasecmp("no", arg.c_str()) == 0;
      if (!yes && !no) ARG_ERROR("du-graph argument must be 'yes' or 'no'.");
      du_graph = yes;
    }
    if (parser.isOpt("time")) {
      show_time = true;
    }

    // Check for other options: Structure recovery
    if (parser.isOpt("include")) {
      searchPathStr += ":" + parser.getOptArg("include");
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

    if (parser.isOpt("show-gaps")) {
      show_gaps = true;
    }

    // Check for other options: Output options
    if (parser.isOpt("output")) {
      out_filenm = parser.getOptArg("output");
    }

    // Check for required arguments
    if (parser.getNumArgs() != 1) {
      ARG_ERROR("Incorrect number of arguments!");
    }
    in_filenm = parser.getArg(0);

    if (out_filenm.empty()) {
      string base_filenm = FileUtil::basename(in_filenm);
      out_filenm = base_filenm + ".hpcstruct";
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
  os << "Args.in_filenm= " << in_filenm << endl;
}


void
Args::ddump() const
{
  dump(std::cerr);
}
