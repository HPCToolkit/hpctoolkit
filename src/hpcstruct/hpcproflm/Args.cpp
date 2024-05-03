// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

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
using std::endl;

#include <string>
using std::string;

//*************************** User Include Files ****************************

#include "../../common/lean/gcc-attr.h"

#include "Args.hpp"

#include "../../common/Util.hpp"

#include "../../common/diagnostics.h"
#include "../../common/Trace.hpp"
#include "../../common/StrUtil.hpp"

//*************************** Forward Declarations **************************

#define ARG_Throw(streamArgs) DIAG_ThrowX(Args::Exception, streamArgs)

// Cf. DIAG_Die.
#define ARG_ERROR(streamArgs)                                        \
  { std::ostringstream WeIrDnAmE;                                    \
    WeIrDnAmE << streamArgs /*<< std::ends*/;                        \
    printError(std::cerr, WeIrDnAmE.str().c_str());                  \
    exit(1); }

//***************************************************************************

static const char* usage_summary =
"<measurement directory>\n";

static const char* usage_details =
     "hpcproflm generates a list of load modules used by profiles\n"
     "in a measurement directory\n"
     "\n"
     "Options:\n"
     "  -V, --version        Print version information.\n"
     "  -h, --help           Print this help.\n"
     ;

#define CLP CmdLineParser

// Note: Changing the option name requires changing the name in Parse()
CmdLineParser::OptArgDesc Args::optArgs[] = {
   // General
  { 'V', "version",         CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL,
     NULL },
  { 'h', "help",            CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL,
     NULL },
  CmdLineParser_OptArgDesc_NULL_MACRO // SGI's compiler requires this version
};

#undef CLP


//***************************************************************************
// Args
//***************************************************************************

Args::Args()
{
}


Args::Args(int argc, const char* const argv[])
{
  parse(argc, argv);
}


Args::~Args()
{
}


void
Args::printUsage(std::ostream& os)
{
  os << "Usage: \n"
     << "  " << getCmd() << " " << usage_summary << endl
     << usage_details << endl;
}


void
Args::printError(std::ostream& os, const char* msg) /*const*/
{
  os << getCmd() << ": " << msg << endl;
  printUsage(os);
}


const std::string&
Args::getCmd() /*const*/
{
  // avoid error messages with: .../bin/hpcproflm-bin
  static string cmd = "hpcproflm";
  return cmd;
}


void
Args::parse(int argc, const char* const argv[])
{
  try {
    // -------------------------------------------------------
    // Parse the command line
    // -------------------------------------------------------
    parser.parse(optArgs, argc, argv);

    // Special options that should be checked first
    if (parser.isOpt("help")) {
      printUsage(std::cerr);
      exit(1);
    }

    // Check for required arguments
    unsigned int numArgs = parser.getNumArgs();
    if (numArgs != 1) {
      ARG_ERROR("Incorrect number of arguments!");
    }

    measurements_directory = parser.getArg(0);
  }
  catch (const CmdLineParser::ParseError& x) {
    ARG_ERROR(x.what());
  }
  catch (const CmdLineParser::Exception& x) {
    DIAG_EMsg(x.message());
    exit(1);
  }
  catch (const Args::Exception& x) {
    ARG_ERROR(x.what());
  }
}
