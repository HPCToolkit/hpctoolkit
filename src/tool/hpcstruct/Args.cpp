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
// Copyright ((c)) 2002-2024, Rice University
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

//************************* System Include Files *******************

#include <iostream>
using std::cerr;
using std::endl;

#include <string>
using std::string;

#include <strings.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdlib>

//*************************** User Include Files ****************************

#include "Args.hpp"

#include "../../include/hpctoolkit-version.h"

#include "../../lib/analysis/Util.hpp"
#include "../../lib/support/diagnostics.h"
#include "../../lib/support/FileUtil.hpp"
#include "../../lib/support/StrUtil.hpp"

//*************************** Forward Declarations **************************

// Cf. DIAG_Die.
#define ARG_ERROR(streamArgs)                                        \
  { std::ostringstream WeIrDnAmE;                                    \
    WeIrDnAmE << streamArgs /*<< std::ends*/;                        \
    printError(std::cerr, WeIrDnAmE.str());                          \
    exit(1); }

//***************************************************************************

// Size in bytes for parallel analysis of binaries
#define DEFAULT_PSIZE     100000000   // 100MB

static const char* usage_summary =
  "  hpcstruct [options] <measurement directory>\n"
  "  hpcstruct [options] <binary>\n";


static const char* usage_details = R"EOF(Description:
  hpcstruct recovers program structure information for CPU and GPU
  binaries.  hpcprof uses program structure information to relate
  performance measurements back to source code.

  Apply hpcstruct to a directory containing HPCToolkit performance
  measurements to recover the program structure for each CPU and GPU
  binary referenced by measurement data. Although not normally
  necessary, one can apply hpcstruct directly to an individual CPU or
  GPU binary to recover its program structure.

  Program structure is a mapping from addresses of machine
  instructions in a binary to source code contexts; this mapping is
  used to attribute measured performance metrics back to source
  code. A strength of hpcstruct is its ability to attribute metrics to
  inlined functions and loops; such mappings are especially useful for
  understanding the performance of programs generated using
  template-based programming models.  When hpcprof is invoked on a
  measurement-directory that contains program structure files, hpcprof
  uses these program structure files to attribute performance measurements.

  hpcstruct is designed to cache its results so that processing
  multiple measurement directories can copy previously generated
  structure files from the cache rather than regenerating the
  information.  A cache may be specified either on the command line
  (see the -c option, below) or by setting the
  HPCTOOLKIT_HPCSTRUCT_CACHE environment variable.  If a cache is
  specified, hpcstruct emits a message for each binary processed
  saying 'Added to cache', 'Replaced in cache', 'Copied from cache',
  or 'Copied from cache +'.  The latter indicates that the structure
  file was found in the cache, but it came from an identical binary with
  a different full path, so the cached content was edited to reflect the
  actual path.  'Replaced in cache' indicates that a previous version at
  that path was replaced.  If the --nocache option is set, hpcstruct will
  not use the cache, even if the environment variable is set; the message
  will say 'Cache disabled by user'.  If no cache is specified, the
  message will say 'Cache not specified' and, unless the --nocache
  option was set, an ADVICE message urging use of a cache will be
  written.  Users are strongly encouraged to use a cache,

  To accelerate analysis of a measurement directory, which contains
  references to an application as well as any shared libraries and/or
  GPU binaries it uses, hpcstruct employs multiple threads by
  default. A pool of threads equal to half of the threads in the CPU
  set for the process is used.  Binaries larger than a certain
  threshold (see the --psize option and its default) are analyzed
  using more threads than those smaller than the threshold.  Multiple
  binaries are processed concurrently.  hpcstruct will describe the
  actual parallelization and concurrency used when the run starts.

  hpcstruct is designed for analysis of optimized binaries created from
  C, C++, Fortran, CUDA, HIP, and DPC++ source code. Because hpcstruct's
  algorithms exploit the line map and debug information recorded in an
  application binary during compilation, for best results, we recommend that
  binaries be compiled with standard debug information or at a minimum,
  line map information. Typically, this is accomplished by passing a '-g'
  option to each compiler along with any optimization flags. See the
  HPCToolkit manual for more information.

Options: General
  -V, --version        Print version information.
  -h, --help           Print this help message.

Options: Control caching of structure files
  -c <dir>, --cache <dir> Specify that structure files for CPU and GPU
                       binaries should be cached and reused in
                       directory <dir>. Overrides any default setting in
                       the HPCTOOLKIT_HPCSTRUCT_CACHE environment variable.
  --nocache            Specify that a structure cache should not be used,
                       even if the HPCTOOLKIT_HPCSTRUCT_CACHE environment
                       variable is set.

Options: Override parallelism defaults
  -j <num>, --jobs <num> Specify the number of threads to be used. <num>
                       OpenMP threads will be used to analyze any large
                       binaries. A pool of <num> threads will be used to
                       analyze small binaries.
  --psize <psize>      hpcstruct will consider any binary of at least
                       <psize> bytes as large. hpcstruct will use more
                       OpenMP threads to analyze large binaries than
                       it uses to analyze small binaries.  {100000000}
  -s <v>, --stack <v>  Set the stack size for OpenMP worker threads to <v>.
                       <v> is a positive number optionally followed by
                       a suffix: B (bytes), K (kilobytes), M (megabytes),
                       or G (gigabytes). Without a suffix, <v> will be
                       interpreted as kilobytes. One can also control the
                       stack size by setting the OMP_STACKSIZE environment
                       variable. A '-s <v>' option takes precedence,
                       followed by OMP_STACKSIZE. {32M}

Options: Override structure recovery defaults
  --cpu <yes/no>       Analyze CPU binaries referenced by a measurements
                       directory. {yes}
  --gpu <yes/no>       Analyze GPU binaries referenced by a measurements
                       directory. {yes}
  --gpucfg <yes/no>    Compute loop nesting structure for GPU machine code.
                       Loop nesting structure is only useful with
                       instruction-level measurements collected using PC
                       sampling or instrumentation. {no}


Options: Specify output file when analyzing a single binary
  -o <file>, --output <file>
                       Write hpcstruct file to <file>.
                       Use '--output=-' to write output to stdout.
                       Note: this option may only be used when analyzing
                       a single binary.

Options: Developers only
  --pretty-print       Add indenting for more readable XML output
  --jobs-struct <num>  Use <num> threads for the MakeStructure() phase only.
  --jobs-parse  <num>  Use <num> threads for the ParseAPI::parse() phase only.
  --jobs-symtab <num>  Use <num> threads for the Symtab phase (if possible).
  --show-gaps          Feature to show unclaimed vma ranges (gaps)
                       in the control-flow graph.
  --time               Display stats on hpcstruct's time and space usage.

Options: Internal use only
  -M <measurement-dir> Indicates that hpcstruct was invoked by a
                       script used to process measurement directory
                       <measurement-dir>. This information is used to
                       control messages.
)EOF";


#define CLP CmdLineParser
#define CLP_SEPARATOR "!!!"

// Note: Changing the option name requires changing the name in Parse()
CmdLineParser::OptArgDesc Args::optArgs[] = {
  { 'j',  "jobs",         CLP::ARG_REQ,  CLP::DUPOPT_CLOB,  NULL,  NULL },
  {  0 ,  "jobs-struct",  CLP::ARG_REQ,  CLP::DUPOPT_CLOB,  NULL,  NULL },
  {  0 ,  "jobs-parse",   CLP::ARG_REQ,  CLP::DUPOPT_CLOB,  NULL,  NULL },
  {  0 ,  "jobs-symtab",  CLP::ARG_REQ,  CLP::DUPOPT_CLOB,  NULL,  NULL },
  {  0 ,  "psize",        CLP::ARG_REQ,  CLP::DUPOPT_CLOB,  NULL,  NULL },
  { 's',  "stack",        CLP::ARG_REQ,  CLP::DUPOPT_CLOB,  NULL,  NULL },
  {  0 ,  "time",         CLP::ARG_NONE, CLP::DUPOPT_CLOB,  NULL,  NULL },
  { 'c',  "cache",        CLP::ARG_REQ,  CLP::DUPOPT_ERR,   NULL,  NULL },
  {  0 ,  "nocache",      CLP::ARG_NONE, CLP::DUPOPT_CLOB,  NULL,  NULL },
  {  0 ,  "pretty-print", CLP::ARG_NONE, CLP::DUPOPT_CLOB,  NULL,  NULL },
  { 'M',  "meas_dir",     CLP::ARG_REQ,  CLP::DUPOPT_ERR,   NULL,  NULL },

  // Structure recovery options
  {  0 ,  "gpucfg",       CLP::ARG_REQ,  CLP::DUPOPT_CLOB,  NULL,  NULL },
  {  1 ,  "gpu",          CLP::ARG_REQ,  CLP::DUPOPT_CLOB,  NULL,  NULL },
  {  1 ,  "cpu",          CLP::ARG_REQ,  CLP::DUPOPT_CLOB,  NULL,  NULL },
  { 'I', "include",       CLP::ARG_REQ,  CLP::DUPOPT_CAT,  ":",
     NULL },
  { 'R', "replace-path",  CLP::ARG_REQ,  CLP::DUPOPT_CAT,  CLP_SEPARATOR,
     NULL},
  {  0 , "show-gaps",     CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL,
     NULL },

  // Output options
  { 'o', "output",        CLP::ARG_REQ , CLP::DUPOPT_CLOB, NULL,
     NULL },

  // General
  { 'v', "verbose",       CLP::ARG_OPT,  CLP::DUPOPT_CLOB, NULL,
     CLP::isOptArg_long },
  { 'V', "version",       CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL,
     NULL },
  { 'h', "help",          CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL,
     NULL },
  {  0 , "debug",         CLP::ARG_OPT,  CLP::DUPOPT_CLOB, NULL,
     CLP::isOptArg_long },
  {  0 , "debug-proc",    CLP::ARG_REQ,  CLP::DUPOPT_CLOB, NULL,
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
  jobs = 0;
  jobs_struct = 0;
  jobs_parse = 0;
  jobs_symtab = 0;
  show_time = false;
  analyze_cpu_binaries = 1;
  analyze_gpu_binaries = 1;
  parallel_analysis_threshold = DEFAULT_PSIZE;
  searchPathStr = ".";
  show_gaps = false;
  nocache = false;
  compute_gpu_cfg = false;
  meas_dir = "";
  is_from_makefile = false;
  cache_stat = CACHE_DISABLED;
  pretty_print_output = false;
}


Args::~Args()
{
}


void
Args::printUsage(std::ostream& os) const
{
  os << "Usage: " << endl
     << usage_summary << endl
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
      hpctoolkit_print_version(getCmd().c_str());
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

    // Stack size to use for OpenMP threads.
    {
      const char* orig_stacksize = std::getenv("OMP_STACKSIZE");
      std::string new_stacksize;
      if (orig_stacksize == nullptr || orig_stacksize[0] == '\0') {
        // OMP_STACKSIZE is empty, we want either the user value or default
        new_stacksize = parser.isOpt("stack") ? parser.getOptArg("stack") : "32M";
      } else if (parser.isOpt("stack")) {
        // If OMP_STACKSIZE is already set, we only override it if it differs from the user value.
        if (parser.getOptArg("stack") != std::string(orig_stacksize)) {
          new_stacksize = parser.getOptArg("stack");
        }
      }

      // If we need to change OMP_STACKSIZE, we need to restart from the beginning of the process
      // because OpenMP will have read the value before now. So set it and re-exec ourselves.
      if (!new_stacksize.empty()) {
        setenv("OMP_STACKSIZE", new_stacksize.c_str(), 1);
        if (argc > 0) {
          execvp(argv[0], (char**)argv);
          std::cerr << getCmd()
                    << ": exec failed, unable to alter OMP_STACKSIZE"
                    << std::endl;
        }
      }
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

    if (parser.isOpt("psize")) {
      const string & arg = parser.getOptArg("psize");
      parallel_analysis_threshold = CmdLineParser::toLong(arg);
    }

    if (parser.isOpt("cache")) {
      const string & arg = parser.getOptArg("cache");
      cache_directory = arg.c_str();
    }

    if (parser.isOpt("nocache")) {
      nocache =  true;
      if (!cache_directory.empty())
        ARG_ERROR("can't specify nocache and a cache directory.");
    }

    if (parser.isOpt("pretty-print")) {
      pretty_print_output = true; // default: false
    }

    if (parser.isOpt("gpucfg")) {
      const string & arg = parser.getOptArg("gpucfg");
      bool yes = strcasecmp("yes", arg.c_str()) == 0;
      bool no = strcasecmp("no", arg.c_str()) == 0;
      if (!yes && !no) ARG_ERROR("gpucfg argument must be 'yes' or 'no'.");
      compute_gpu_cfg = yes;
    }

    if (parser.isOpt("gpu")) {
      const string & arg = parser.getOptArg("gpu");
      bool yes = strcasecmp("yes", arg.c_str()) == 0;
      bool no = strcasecmp("no", arg.c_str()) == 0;
      if (!yes && !no) ARG_ERROR("gpu argument must be 'yes' or 'no'.");
      analyze_gpu_binaries = yes;
    }

    if (parser.isOpt("cpu")) {
      const string & arg = parser.getOptArg("cpu");
      bool yes = strcasecmp("yes", arg.c_str()) == 0;
      bool no = strcasecmp("no", arg.c_str()) == 0;
      if (!yes && !no) ARG_ERROR("cpu argument must be 'yes' or 'no'.");
      analyze_cpu_binaries = yes;
    }

    if (parser.isOpt("meas_dir")) {
      const string & arg = parser.getOptArg("meas_dir");
      meas_dir = arg.c_str();
      is_from_makefile = true;
    }
    if (parser.isOpt("time")) {
      show_time = true;
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

    struct stat sb;
    if (stat(in_filenm.c_str(), &sb) == 0 && !S_ISDIR(sb.st_mode)) {
      if (out_filenm.empty()) {
        string base_filenm = FileUtil::basename(in_filenm);
        out_filenm = base_filenm + ".hpcstruct";
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
  os << "Args.in_filenm= " << in_filenm << endl;
}


void
Args::ddump() const
{
  dump(std::cerr);
}
