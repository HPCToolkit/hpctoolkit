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
// Copyright ((c)) 2002-2022, Rice University
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

//*************************** User Include Files ****************************

#include <include/hpctoolkit-config.h>

#include "Args.hpp"

#include <lib/analysis/Util.hpp>
#include <lib/support/diagnostics.h>
#include <lib/support/FileUtil.hpp>
#include <lib/support/StrUtil.hpp>

//*************************** Forward Declarations **************************

// Cf. DIAG_Die.
#define ARG_ERROR(streamArgs)					     \
  { std::ostringstream WeIrDnAmE;                                    \
    WeIrDnAmE << streamArgs /*<< std::ends*/;                        \
    printError(std::cerr, WeIrDnAmE.str());                          \
    exit(1); }

//***************************************************************************

// Size in bytes for parallel analysis of binaries
#define DEFAULT_PSIZE     100000000   // 100MB

static const char* version_info = HPCTOOLKIT_VERSION_STRING;

static const char* usage_summary =
  "  hpcstruct [options] <measurement directory>\n"
  "  hpcstruct [options] <binary>\n";


static const char* usage_details =
#include "usage.h"
;


#define CLP CmdLineParser
#define CLP_SEPARATOR "!!!"

// Note: Changing the option name requires changing the name in Parse()
CmdLineParser::OptArgDesc Args::optArgs[] = {
  { 'j',  "jobs",         CLP::ARG_REQ,  CLP::DUPOPT_CLOB,  NULL,  NULL },
  {  0 ,  "jobs-struct",  CLP::ARG_REQ,  CLP::DUPOPT_CLOB,  NULL,  NULL },
  {  0 ,  "jobs-parse",   CLP::ARG_REQ,  CLP::DUPOPT_CLOB,  NULL,  NULL },
  {  0 ,  "jobs-symtab",  CLP::ARG_REQ,  CLP::DUPOPT_CLOB,  NULL,  NULL },
  {  0 ,  "psize",        CLP::ARG_REQ,  CLP::DUPOPT_CLOB,  NULL,  NULL },
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
Args::printVersion(std::ostream& os) const
{
  os << getCmd() << ": " << version_info << endl;
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
#if 0
      fprintf(stderr, "DEBUG meas_dir = %s; is_from_makefile set to true\n", meas_dir.c_str() );
#endif
    }
    if (parser.isOpt("time")) {
      show_time = true;
    }

#if 0
    // Check for other options: Structure recovery
    if (parser.isOpt("include")) {
      searchPathStr += ":" + parser.getOptArg("include");
    }
    if (parser.isOpt("replace-path")) {
      string arg = parser.getOptArg("replace-path");

      std::vector<std::string> replacePaths;
      StrUtil::tokenize_str(arg, CLP_SEPARATOR, replacePaths);

      for (unsigned int i = 0; i < replacePaths.size(); ++i) {
	int occurancesOfEquals =
	  Analysis::Util::parseReplacePath(replacePaths[i]);

	if (occurancesOfEquals > 1) {
	  ARG_ERROR("Too many occurrences of \'=\'; make sure to escape any \'=\' in your paths");
	}
	else if(occurancesOfEquals == 0) {
	  ARG_ERROR("The \'=\' between the old path and new path is missing");
	}
      }
    }
#endif

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
#if 0
    fprintf(stderr, "DEBUG in_filenm = `%s', is_from_makefile = %s\n",
	in_filenm.c_str(),
	(is_from_makefile == true ? "true" : (is_from_makefile == false? "false" : "bad value" ) ) );
#endif

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
