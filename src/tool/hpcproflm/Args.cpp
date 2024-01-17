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
// Copyright ((c)) 2002-2023, Rice University
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
using std::endl;

#include <string>
using std::string;

//*************************** User Include Files ****************************

#include "hpctoolkit-config.h"
#include "../../include/gcc-attr.h"

#include "Args.hpp"

#include "../../lib/analysis/Util.hpp"

#include "../../lib/support/diagnostics.h"
#include "../../lib/support/Trace.hpp"
#include "../../lib/support/StrUtil.hpp"

//*************************** Forward Declarations **************************

#define ARG_Throw(streamArgs) DIAG_ThrowX(Args::Exception, streamArgs)

// Cf. DIAG_Die.
#define ARG_ERROR(streamArgs)                                        \
  { std::ostringstream WeIrDnAmE;                                    \
    WeIrDnAmE << streamArgs /*<< std::ends*/;                        \
    printError(std::cerr, WeIrDnAmE.str().c_str());                  \
    exit(1); }

//***************************************************************************

static const char* version_info = HPCTOOLKIT_VERSION_STRING;

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
Args::printVersion(std::ostream& os) const
{
  os << getCmd() << ": " << version_info << endl;
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
