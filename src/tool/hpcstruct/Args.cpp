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

//*************************** User Include Files ****************************

#include <include/hpctoolkit-config.h>

#include "Args.hpp"

#include <lib/analysis/Util.hpp>
#include <lib/banal/Struct.hpp>

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

static const char* version_info = HPCTOOLKIT_VERSION_STRING;

static const char* usage_summary =
"[options] <binary>\n";

static const char* usage_details = "\
Given an application binary or DSO <binary>, hpcstruct recovers the program\n\
structure of its object code.  Program structure is a mapping of a program's\n\
static source-level structure to its object code.  By default, hpcstruct\n\
writes its results to the file 'basename(<binary>).hpcstruct'.  This file\n\
is typically passed to HPCToolkit's correlation tool hpcprof.\n\
\n\
hpcstruct is designed primarily for highly optimized binaries created from\n\
C, C++ and Fortran source code. Because hpcstruct's algorithms exploit a\n\
binary's debugging information, for best results, binary should be compiled\n\
with standard debugging information.  See the documentation for more\n\
information.\n\
\n\
Options: General\n\
  -v [<n>], --verbose [<n>]\n\
                       Verbose: generate progress messages to stderr at\n\
                       verbosity level <n>. {1}\n\
  -V, --version        Print version information.\n\
  -h, --help           Print this help.\n\
  --debug=[<n>]        Debug: use debug level <n>. {1}\n\
  --debug-proc <glob>  Debug structure recovery for procedures matching\n\
                       the procedure glob <glob>\n\
\n\
Options: Structure recovery\n\
  -I <path>, --include <path>\n\
                       Use <path> when resolving source file names. For a\n\
                       recursive search, append a '*' after the last slash,\n\
                       e.g., '/mypath/*' (quote or escape to protect from\n\
                       the shell.) May pass multiple times.\n\
  --loop-intvl <yes|no>\n\
                       Should loop recovery heuristics assume an irreducible\n\
                       interval is a loop? {yes}\n\
  --loop-fwd-subst <yes|no>\n\
                       Should loop recovery heuristics assume forward\n\
                       substitution may occur? {yes}\n\
  -N <all|safe|none>, --normalize <all|safe|none>\n\
                       Specify normalizations to apply to structure. {all}\n\
                         all : apply all normalizations\n\
                         safe: apply only safe normalizations\n\
                         none: apply no normalizations\n\
  -R '<old-path>=<new-path>', --replace-path '<old-path>=<new-path>'\n\
                       Substitute instances of <old-path> with <new-path>;\n\
                       apply to all paths (profile's load map, source code)\n\
                       for which <old-path> is a prefix.  Use '\\' to escape\n\
                       instances of '=' within a path. May pass multiple\n\
                       times.\n\
  --use-binutils       Use binutils as the default binary instruction decoder\n\
                       On x86 default is Intel XED library.\n\
  --cfg <old|new>      Use old (OpenAnalysis) or new (ParseAPI) support\n\
                       for building Control Flow Graphs (default new).\n\
\n\
Options: Demangling\n\
  --demangle-library <path to demangling library>\n\
                       Specify the pathname for a dynamically-linked\n\
                       library whose demangler function should \n\
                       be used for demangling. By default, the demangler used\n\
                       is __cxa_demangle in the C++ Standard Library linked into\n\
                       hpcstruct.\n\
\n\
  --demangle-function <name of the demangler>\n\
                       By default, the demangler used is __cxa_demangle, a function\n\
                       provided by the C++ Standard Library. This option enables\n\
                       one to specify an alternate demangler, e.g., cplus_demangle\n\
                       provided by the BFD library.\n\
\n\
Options: Output:\n\
  -o <file>, --output <file>\n\
                       Write hpcstruct file to <file>.\n\
                       Use '--output=-' to write output to stdout.\n\
  --compact            Generate compact output, eliminating extra white space\n\
  --dot                Generate dot (graphviz) output file.\n\
  --dot-file <file>    Write dot output to <file>, implies --dot.\n\
                       Use '--dot-file=-' to write output to stdout.\n\
";

// Possible extensions:
//  --Li : Select the opposite of the --loop-intvl default.
//  --Lf : Select the opposite of the --loop-fwd-subst default.


#define CLP CmdLineParser
#define CLP_SEPARATOR "!!!"

// Note: Changing the option name requires changing the name in Parse()
CmdLineParser::OptArgDesc Args::optArgs[] = {
  {  0 , "agent-c++",       CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL,
     NULL },
  {  0 , "agent-cilk",      CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL,
     NULL },

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

  { 'N', "normalize",       CLP::ARG_REQ,  CLP::DUPOPT_CLOB, NULL,
     NULL },

  { 'R', "replace-path",    CLP::ARG_REQ,  CLP::DUPOPT_CAT,  CLP_SEPARATOR,
     NULL},

  // Output options
  { 'o', "output",          CLP::ARG_REQ , CLP::DUPOPT_CLOB, NULL,
     NULL },
  {  0 , "compact",         CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL,
     NULL },
  {  0 , "dot",             CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL,
     NULL },
  {  0 , "dot-file",        CLP::ARG_REQ , CLP::DUPOPT_CLOB, NULL,
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
  {  0 , "cfg",            CLP::ARG_REQ ,  CLP::DUPOPT_CLOB, NULL,
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
  searchPathStr = ".";
  isIrreducibleIntervalLoop = true;
  isForwardSubstitution = true;
  doNormalizeTy = BAnal::Struct::NormTy_All;
  doDot = false;
  prettyPrintOutput = true;
  useBinutils = false;
  cfgRequest = BAnal::Struct::CFG_DEFAULT;
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
    if (parser.isOpt("normalize")) {
      const string& arg = parser.getOptArg("normalize");
      doNormalizeTy = parseArg_norm(arg, "--normalize option");
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

    // Instruction decoder options
    useBinutils = parser.isOpt("use-binutils");

    if (parser.isOpt("cfg")) {
      string arg = parser.getOptArg("cfg");
      if (arg == "old") { cfgRequest = BAnal::Struct::CFG_OA; }
      else if (arg == "new") { cfgRequest = BAnal::Struct::CFG_PARSEAPI; }
      else {
	DIAG_EMsg("unknown argument for --cfg (old|new): '" << arg << "'");
	exit(1);
      }
    }

    // Check for other options: Demangling
    if (parser.isOpt("demangle-library")) {
      demangle_library = parser.getOptArg("demangle-library");
    }


    // Check for other options: Demangling
    if (parser.isOpt("demangle-function")) {
      demangle_function = parser.getOptArg("demangle-function");
    }

    // Check for other options: Output options
    if (parser.isOpt("output")) {
      out_filenm = parser.getOptArg("output");
    }
    if (parser.isOpt("compact")) {
      prettyPrintOutput = false;
    }
    if (parser.isOpt("dot")) {
      doDot = true;
    }
    if (parser.isOpt("dot-file")) {
      dot_filenm = parser.getOptArg("dot-file");
      doDot = true;
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


//***************************************************************************

BAnal::Struct::NormTy
Args::parseArg_norm(const string& value, const char* err_note)
{
  if (value == "all") {
    return BAnal::Struct::NormTy_All;
  }
  else if (value == "safe") {
    return BAnal::Struct::NormTy_Safe;
  }
  else if (value == "none") {
    return BAnal::Struct::NormTy_None;
  }
  else {
    ARG_ERROR(err_note << ": Unexpected value received: " << value);
  }
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

