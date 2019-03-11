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
// Copyright ((c)) 2002-2019, Rice University
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
#include <dirent.h>
#include <sys/stat.h>

#include <vector>
#include <string>
using std::string;

//*************************** User Include Files ****************************

#include <include/hpctoolkit-config.h>

#include "Args.hpp"

#include <lib/analysis/Util.hpp>
#include <lib/support/diagnostics.h>
#include <lib/support/FileUtil.hpp>
#include <lib/support/StrUtil.hpp>
#include <lib/support/realpath.h>

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
"[options] <binary | executable | measurement directory>\n";

static const char* usage_details = 
#include "usage.h"
;

// Possible extensions:
//  --Li : Select the opposite of the --loop-intvl default.
//  --Lf : Select the opposite of the --loop-fwd-subst default.

static const char* cubins_analysis_makefile =
#include "cubins-analysis.h"
;


#define CLP CmdLineParser
#define CLP_SEPARATOR "!!!"

// Note: Changing the option name requires changing the name in Parse()
CmdLineParser::OptArgDesc Args::optArgs[] = {
  {  0 , "agent-c++",       CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL,
     NULL },
  {  0 , "agent-cilk",      CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL,
     NULL },

  { 'j',  "jobs",  CLP::ARG_REQ,  CLP::DUPOPT_CLOB,  NULL,  NULL },
  {  0 ,  "jobs-parse",   CLP::ARG_REQ,  CLP::DUPOPT_CLOB,  NULL,  NULL },
  {  0 ,  "jobs-symtab",  CLP::ARG_REQ,  CLP::DUPOPT_CLOB,  NULL,  NULL },
  {  0 ,  "time",         CLP::ARG_NONE, CLP::DUPOPT_CLOB,  NULL,  NULL },

  // Demangler library
  {  0 , "demangle-library",  CLP::ARG_REQ,  CLP::DUPOPT_CLOB, NULL,
     NULL },

  // Demangler function
  {  0 , "demangle-function",  CLP::ARG_REQ,  CLP::DUPOPT_CLOB, NULL,
     NULL },

  // Structure recovery options
  { 'I', "include",         CLP::ARG_REQ,  CLP::DUPOPT_CAT,  ":",
     NULL },
  {  0 , "loop-intvl",      CLP::ARG_REQ,  CLP::DUPOPT_CLOB, NULL,
     NULL },
  {  0 , "loop-fwd-subst",  CLP::ARG_REQ,  CLP::DUPOPT_CLOB, NULL,
     NULL },
  { 'R', "replace-path",    CLP::ARG_REQ,  CLP::DUPOPT_CAT,  CLP_SEPARATOR,
     NULL},
  {  0 , "show-gaps",       CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL,
     NULL },

  // Output options
  { 'o', "output",          CLP::ARG_REQ , CLP::DUPOPT_CLOB, NULL,
     NULL },
  {  0 , "compact",         CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL,
     NULL },

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

  // Instruction decoder options
  { 0, "use-binutils",     CLP::ARG_NONE,  CLP::DUPOPT_CLOB, NULL,
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
  jobs_parse = -1;
  jobs_symtab = -1;
  show_time = false;
  searchPathStr = ".";
  isIrreducibleIntervalLoop = true;
  isForwardSubstitution = true;
  prettyPrintOutput = true;
  useBinutils = false;
  show_gaps = false;
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
  static std::string command = std::string("hpcstruct");
  return  command;
}


static inline bool is_directory(const std::string &path) {
  struct stat statbuf;
  if (stat(path.c_str(), &statbuf) != 0)
    return 0;
  return S_ISDIR(statbuf.st_mode);
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
    if (parser.isOpt("debug-proc")) {
      dbgProcGlob = parser.getOptArg("debug-proc");
    }

    // Number of openmp threads (jobs, jobs-parse)
    if (parser.isOpt("jobs")) {
      const string & arg = parser.getOptArg("jobs");
      jobs = (int) CmdLineParser::toLong(arg);
    }
    if (parser.isOpt("jobs-parse")) {
      const string & arg = parser.getOptArg("jobs-parse");
      jobs_parse = (int) CmdLineParser::toLong(arg);
    }
    if (parser.isOpt("jobs-symtab")) {
      const string & arg = parser.getOptArg("jobs-symtab");
      jobs_symtab = (int) CmdLineParser::toLong(arg);
    }
    if (parser.isOpt("time")) {
      show_time = true;
    }

    // Check for LUSH options (TODO)
    if (parser.isOpt("agent-c++")) {
      lush_agent = "agent-c++";
    }
    if (parser.isOpt("agent-cilk")) {
      lush_agent = "agent-cilk";
    }

    // Check for other options: Structure recovery
    if (parser.isOpt("include")) {
      searchPathStr += ":" + parser.getOptArg("include");
    }
    if (parser.isOpt("loop-intvl")) {
      const string& arg = parser.getOptArg("loop-intvl");
      isIrreducibleIntervalLoop =
	CmdLineParser::parseArg_bool(arg, "--loop-intvl option");
    }
    if (parser.isOpt("loop-fwd-subst")) {
      const string& arg = parser.getOptArg("loop-fwd-subst");
      isForwardSubstitution =
	CmdLineParser::parseArg_bool(arg, "--loop-fwd-subst option");
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

    // Instruction decoder options
    useBinutils = parser.isOpt("use-binutils");

    // Check for other options: Demangling
    if (parser.isOpt("demangle-library")) {
      demangle_library = parser.getOptArg("demangle-library");
    }

    // Check for other options: Demangling
    if (parser.isOpt("demangle-function")) {
      demangle_function = parser.getOptArg("demangle-function");
    }

    std::string output_name;
    // Check for other options: Output options
    if (parser.isOpt("output")) {
      output_name = parser.getOptArg("output");
    }
    if (parser.isOpt("compact")) {
      prettyPrintOutput = false;
    }

    // Check for required arguments
    if (parser.getNumArgs() != 1) {
      ARG_ERROR("Incorrect number of arguments!");
    }

    std::string input_name = parser.getArg(0);

    if (is_directory(input_name)) {
      auto input_path = std::string(RealPath(input_name.c_str()));
      auto cubins_dir = input_path + "/cubins";
      DIR *dir;
      if ((dir = opendir(cubins_dir.c_str())) != NULL) {
#if 0
	auto structs_dir = input_path + "/structs";
        mkdir(structs_dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	struct dirent *ent;
        /* append files within the directory */
        while ((ent = readdir(dir)) != NULL) {
          std::string file_name = std::string(ent->d_name);
          if (!is_directory(cubins_dir + "/" + file_name)) {
            in_filenm.push_back(cubins_dir + "/" + file_name);
            if (output_name.size() == 0) {
              out_filenm.push_back(structs_dir + "/" + file_name + ".hpcstruct");
            } else {
              if (output_name == "-") {
                out_filenm.push_back("-");
              } else {
                out_filenm.push_back(output_name + "/" + file_name + ".hpcstruct");
              }
            }
          }
        }
#else

	struct dirent *ent;
	bool cubins_found = false;
        std::string cubin_suffix = std::string("cubin");
        while ((ent = readdir(dir)) != NULL) {
          std::string file_name = std::string(ent->d_name);
          if (file_name.find(cubin_suffix) != std::string::npos) {
            cubins_found = true;
            break;
          }
        }

        if (!cubins_found) {
	  ARG_ERROR("specified directory is not a hpctoolkit measurement directory containing cubins");
        }

	auto structs_dir = input_path + "/structs";
        mkdir(structs_dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

	// use "make" to launch parallel analysis of a directory full of cubin files
	auto makefile_name = structs_dir + "/Makefile";
        FILE *makefile = fopen(makefile_name.c_str(),"w");
        if (makefile) {
          size_t len =  strlen(cubins_analysis_makefile);
          size_t bytes_written = fwrite(cubins_analysis_makefile, (size_t) 1, len, makefile);
          if (bytes_written == len) {
	    fclose(makefile);

	    // see cubins-analysis.txt in this directory to understand this command
            std::string make_cmd = 
	      "make -C " + structs_dir + " CUBINS_DIR=" + cubins_dir + 
              " STRUCTS_DIR=" + structs_dir + " --no-print-directory";

            system(make_cmd.c_str());
          } else {
            fclose(makefile);
          }
        }
#endif
        closedir(dir);
      } else {
	  ARG_ERROR("specified directory is not a hpctoolkit measurement directory containing cubins");
      }
    } else {
      in_filenm.push_back(std::string(input_name));
      if (output_name.size() == 0) {
        string base_filenm = FileUtil::basename(input_name);
        out_filenm.push_back(base_filenm + ".hpcstruct");
      } else {
        out_filenm.push_back(output_name);
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
  for (auto &input_name : in_filenm) {
    os << "Args.in_filenm= " << input_name << endl;
  }
}


void
Args::ddump() const
{
  dump(std::cerr);
}


//***************************************************************************

#if 0
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
#endif

