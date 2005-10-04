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
//    Args.C
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

//************************* System Include Files ****************************

#ifdef NO_STD_CHEADERS
# include <stdlib.h>
#else
# include <cstdlib>
using std::strtol; // For compatibility with non-std C headers
#endif

#include <errno.h>

//*************************** User Include Files ****************************

#include "Args.h"
#include <lib/support/Trace.h>

//*************************** Forward Declarations **************************

using std::cerr;
using std::endl;
using std::string;

//***************************************************************************

static const char* version_info =
#include <include/HPCToolkitVersionInfo.h>

static const char* usage_summary =
"[options] <loadmodule>\n";

static const char* usage_details =
"Load module dump.  Dumps selected contents of <loadmodule> to stdout.\n"
"<loadmodule> may be either an executable or DSO.\n"
"\n"
"By default, section, procedure and instruction lists are dumped.\n"
"\n"
"Options:\n"
"  -s, --symbolic-info  Instead of the default, dump symbolic info\n"
"                       (file, func, line) associated with each PC/VMA.\n"
"  -t, --symbolic-info-old\n"
"                       Instead of the default, dump symbolic info (old).\n"
"  -l <addr>, load-addr <addr>\n"
"                       By default, DSOs will be 'loaded' at 0x0.  Use this\n"
"                       option to specify a different load address.\n"
"                       Addresses may be in base 10, 8 (prefix '0') or 16\n"
"                       (prefix '0x').  [NOT FULLY IMPLEMENTED]\n"
"  -V, --version        Print version information.\n"
"  -h, --help           Print this help.\n";


#define CLP CmdLineParser

// Note: Changing the option name requires changing the name in Parse()
CmdLineParser::OptArgDesc Args::optArgs[] = {

  // Options
  { 's', "symbolic-info",     CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL },
  { 't', "symbolic-info-old", CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL },

  { 'l', "load-addr",     CLP::ARG_REQ , CLP::DUPOPT_CLOB, NULL },
  
  { 'V', "version",  CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL },
  { 'h', "help",     CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL },
  {  0 , "debug",    CLP::ARG_OPT,  CLP::DUPOPT_CLOB, NULL }, // hidden
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
  symbolicDump = false;
  symbolicDumpOld = false;
  loadAddr = 0x0;
  debugLevel = 0;
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
    trace = debugLevel = 0;
    
    if (parser.IsOpt("debug")) { 
      trace = debugLevel = 1; 
      if (parser.IsOptArg("debug")) {
	const string& arg = parser.GetOptArg("debug");
	trace = debugLevel = (int)CmdLineParser::ToLong(arg);
      }
    }
    if (parser.IsOpt("help")) { 
      PrintUsage(std::cerr); 
      exit(1);
    }
    if (parser.IsOpt("version")) { 
      PrintVersion(std::cerr);
      exit(1);
    }
    
    // Check for other options
    if (parser.IsOpt("symbolic-info")) { 
      symbolicDump = true;
    } 
    if (parser.IsOpt("symbolic-info-old")) { 
      symbolicDumpOld = true;
    } 
    if (parser.IsOpt("load-addr")) { 
      const string& arg = parser.GetOptArg("load-addr");
      loadAddr = CmdLineParser::ToLong(arg);

#if 0
      errno = 0;
      long l = strtol(str.c_str(), NULL, 0 /* base: dec, hex, or oct */);
      if (l <= 0 || errno != 0) {
	PrintError(std::cerr, "Invalid address given to -r\n");
	exit(1);
      }
      loadAddr = (Addr)l;
#endif
    }
    
    // Check for required arguments
    if (parser.GetNumArgs() != 1) {
      PrintError(std::cerr, "Incorrect number of arguments!");
      exit(1);
    }
    inputFile = parser.GetArg(0);
  }
  catch (CmdLineParser::ParseError& e) {
    PrintError(std::cerr, e.GetMessage());
    exit(1);
  }
  catch (CmdLineParser::Exception& e) {
    e.Report(std::cerr);
    exit(1);
  }
}


void 
Args::Dump(std::ostream& os) const
{
  os << "Args.cmd= " << GetCmd() << endl; 
  os << "Args.symbolicDump= " << symbolicDump << endl;
  os << "Args.debugLevel= " << debugLevel << endl;
  os << "Args.inputFile= " << inputFile << endl;
  os << "::trace " << ::trace << endl; 
}

void 
Args::DDump() const
{
  Dump(std::cerr);
}
