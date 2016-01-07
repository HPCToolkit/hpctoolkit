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

#ifdef NO_STD_CHEADERS
# include <stdlib.h>
#else
# include <cstdlib>
using std::strtol; // For compatibility with non-std C headers
#endif

#include <errno.h>

//*************************** User Include Files ****************************

#include <include/hpctoolkit-config.h>

#include "Args.hpp"

#include <lib/support/diagnostics.h>
#include <lib/support/Trace.hpp>

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
"[options] <loadmodule>\n";

static const char* usage_details = "\
Load module dump.  Dumps selected contents of <loadmodule> to stdout.\n\
<loadmodule> may be either an executable or DSO.\n\
\n\
By default instruction types are emitted.\
\n\
Options:\n\
  --long               Long dump: include symbol table.\n\
  --short              Short dump: no instructions.\n\
  --decode             Decode instructions.\n\
  --old                Old symbolic dump.\n\
  -l <addr>, load-addr <addr>\n\
                       'Load' DSOs at address <addr> rather than 0x0.\n\
                       Addresses may be in base 10, 8 (prefix '0') or 16\n\
                       (prefix '0x').  [NOT FULLY IMPLEMENTED]\n\
  -V, --version        Print version information.\n\
  -h, --help           Print this help.\n\
  --debug [<n>]        Debug: use debug level <n>. {1}\n";



#define CLP CmdLineParser

// Note: Changing the option name requires changing the name in Parse()
CmdLineParser::OptArgDesc Args::optArgs[] = {

  // Options
  {  0 , "long",    CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL, NULL },
  {  0 , "short",   CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL, NULL },
  {  0 , "decode",  CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL, NULL },
  {  0 , "old",     CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL, NULL },

  { 'l', "load-addr", CLP::ARG_REQ , CLP::DUPOPT_CLOB, NULL, NULL },
  
  { 'V', "version",  CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL, NULL },
  { 'h', "help",     CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL, NULL },
  {  0 , "debug",    CLP::ARG_OPT,  CLP::DUPOPT_CLOB, NULL, CLP::isOptArg_long }, // hidden
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
  dumpLong  = false;
  dumpShort = false;
  dumpDecode = false;
  dumpOld   = false;
  loadVMA = 0x0;
  debugLevel = 0;
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
    trace = debugLevel = 0;
    
    if (parser.isOpt("debug")) {
      trace = debugLevel = 1;
      if (parser.isOptArg("debug")) {
	const string& arg = parser.getOptArg("debug");
	trace = debugLevel = (int)CmdLineParser::toLong(arg);
      }
    }
    if (parser.isOpt("help")) {
      printUsage(std::cerr);
      exit(1);
    }
    if (parser.isOpt("version")) {
      printVersion(std::cerr);
      exit(1);
    }
    
    // Check for other options
    int numDumpOptions = 0;
    if (parser.isOpt("long")) {
      dumpLong = true;
      numDumpOptions++;
    }
    if (parser.isOpt("short")) {
      dumpShort = true;
      numDumpOptions++;
    }
    if (parser.isOpt("decode")) {
      dumpDecode = true;
      if (dumpShort) {
	ARG_ERROR("--decode not valid with --short!");
      }
    }
    if (parser.isOpt("old")) {
      dumpOld = true;
      numDumpOptions++;
    }
    if (numDumpOptions > 1) {
      ARG_ERROR("At most one dump option may be given!");
    }

    if (parser.isOpt("load-addr")) {
      const string& arg = parser.getOptArg("load-addr");
      loadVMA = CmdLineParser::toLong(arg);

#if 0
      errno = 0;
      long l = strtol(str.c_str(), NULL, 0 /* base: dec, hex, or oct */);
      if (l <= 0 || errno != 0) {
	ARG_ERROR("Invalid address given to -r\n");
	exit(1);
      }
      loadVMA = (VMA)l;
#endif
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
  os << "Args.debugLevel= " << debugLevel << endl;
  os << "Args.inputFile= " << inputFile << endl;
  os << "::trace " << ::trace << endl;
}

void
Args::ddump() const
{
  dump(std::cerr);
}
