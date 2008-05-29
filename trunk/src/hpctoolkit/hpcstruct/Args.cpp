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

// Cf. DIAG_Die.
#define ARG_ERROR(streamArgs)                                        \
  { std::ostringstream WeIrDnAmE;                                    \
    WeIrDnAmE << streamArgs /*<< std::ends*/;                        \
    printError(std::cerr, WeIrDnAmE.str());                          \
    exit(1); }

//***************************************************************************

static const char* version_info =
#include <include/HPCToolkitVersionInfo.h>

static const char* usage_summary =
"[options] <binary>\n";

static const char* usage_details = "\
Given an application binary or DSO <binary>, hpcstruct recovers the program\n\
structure of its object code and writes to standard output a program\n\
structure to object code mapping. hpcstruct is designed primarily for highly\n\
optimized binaries created from C, C++ and Fortran source code. Because\n\
hpcstruct's algorithms exploit a binary's debugging information, for best\n\
results, binary should be compiled with standard debugging information.\n\
hpcstruct's output is typically passed to an HPCToolkit's correlation tool.\n\
See the documentation for more information.\n\
\n\
Options: General\n\
  -v [<n>], --verbose [<n>]\n\
                       Verbose: generate progress messages to stderr at\n\
                       verbosity level <n>. {1}\n\
  -V, --version        Print version information.\n\
  -h, --help           Print this help.\n\
  --debug [<n>]        Debug: use debug level <n>. {1}\n\
\n\
Options: Recovery and Output\n\
  -i, --irreducible-interval-as-loop-off\n\
                       Do not treat irreducible intervals as loops\n\
  -f, --forward-substitution-off\n\
                       Assume that forward substitution does not occur.\n\
                       (Useful for handling erroneous PGI debugging info.)\n\
  -p <list>, --canonical-paths <list>\n\
                       Ensure that scope tree only contains files found in\n\
                       the colon-separated <list>. May be passed multiple\n\
                       times.\n\
  -n, --normalize-off  Turn off scope tree normalization\n\
  -u, --unsafe-normalize-off\n\
                       Turn off potentially unsafe normalization\n\
  -c, --compact        Generate compact output, eliminating extra white space\n\
";


#define CLP CmdLineParser

// Note: Changing the option name requires changing the name in Parse()
CmdLineParser::OptArgDesc Args::optArgs[] = {
  // Options
  { 'i', "irreducible-interval-as-loop-off",
                            CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL,
     NULL },
  { 'f', "forward-substitution-off",
                            CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL,
     NULL },
  { 'p', "canonical-paths", CLP::ARG_REQ , CLP::DUPOPT_CAT,  ":",
     NULL },

  { 'n', "normalize-off",   CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL,
     NULL },
  { 'u', "unsafe-normalize-off", 
                            CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL,
     NULL },
  { 'c', "compact",         CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL,
     NULL },
  
  // Options
  { 'v', "verbose",     CLP::ARG_OPT,  CLP::DUPOPT_CLOB, NULL,
     NULL },
  { 'V', "version",     CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL,
     NULL },
  { 'h', "help",        CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL,
     NULL },
  {  0 , "debug",           CLP::ARG_OPT,  CLP::DUPOPT_CLOB, NULL,  // hidden
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
  irreducibleIntervalIsLoop = true;
  forwardSubstitutionOff = false;
  normalizeScopeTree = true;
  unsafeNormalizations = true;
  prettyPrintOutput = true;
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
    
    // Check for other options
    if (parser.isOpt("irreducible-interval-as-loop-off")) { 
      irreducibleIntervalIsLoop = false;
    } 
    if (parser.isOpt("forward-substitution-off")) { 
      forwardSubstitutionOff = true;
    } 
    if (parser.isOpt("canonical-paths")) { 
      canonicalPathList = parser.getOptArg("canonical-paths");
    }
    if (parser.isOpt("normalize-off")) { 
      normalizeScopeTree = false;
    } 
    if (parser.isOpt("unsafe-normalize-off")) { 
      unsafeNormalizations = false;
    } 
    if (parser.isOpt("compact")) { 
      prettyPrintOutput = false;
    } 
    
    // Check for required arguments
    if (parser.getNumArgs() != 1) {
      ARG_ERROR("Incorrect number of arguments!");
    }
    inputFile = parser.getArg(0);
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
  os << "Args.prettyPrintOutput= " << prettyPrintOutput << endl;
  os << "Args.normalizeScopeTree= " << normalizeScopeTree << endl;
  os << "Args.canonicalPathList= " << canonicalPathList << endl;
  os << "Args.inputFile= " << inputFile << endl;
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

