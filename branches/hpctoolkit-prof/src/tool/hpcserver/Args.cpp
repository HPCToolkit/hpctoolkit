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
// Copyright ((c)) 2002-2015, Rice University
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
//   Parses the arguments from the command line
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

#include <lib/support/CmdLineParser.hpp>
#include <lib/support/diagnostics.h>
#include "Args.hpp"

//*************************** Forward Declarations **************************

// Cf. DIAG_Die.
#define ARG_ERROR(streamArgs)                                        \
  { std::ostringstream WeIrDnAmE;                                    \
    WeIrDnAmE << streamArgs /*<< std::ends*/;                        \
    printError(std::cerr, WeIrDnAmE.str());                          \
    exit(1); }

//***************************************************************************

static const char* version_info = "hpcserver version 0.9, protocol version 0.9.\n"
HPCTOOLKIT_VERSION_STRING;

static const char* usage_summary =
"[options]\n";

static const char* usage_details = "\
hpcserver is an optional component that works with hpctraceviewer to enable\n\
visualizations of larger databases. The disk access and heavy processing\n\
required by hpctraceviewer are offloaded to hpctraceserver, which can run in\n\
parallel with MPI. The results are streamed back to a connected hpctraceviewer\n\
client in a way that permits the same interactivity as running hpctraceviewer\n\
with the data located locally.\n\
\n\
Unlike standard web servers, for example, hpcserver is designed to run only\n\
while in use, and not perpetually as a daemon on background process. hpcserver\n\
also runs with the permissions of the user that launched it and allows the\n\
hpctraceviewer client to specify and access any database that hpcserver can read.\n\
\n\
Options:\n\
  -V, --version        Print version information.\n\
  -h, --help           Print this help.\n\
  -c, --compression    Enables or disables compression (on by default)\n\
                          Allowed values: on, off \n\
  -p, --port           Sets the main communication port (default is 21590)\n\
                          Specifying 0 indicates that an open port should be \n\
                          chosen automatically.\n\
  -x, --xmlport        Sets the port on which the experiment.xml file is\n\
                          transferred.  By default, the main port is used,\n\
                          but this can be changed by '-x <port>'.\n\
  --stay-open yes|no   By default, hpcserver resumes listening on the main port\n\
                          for another connection after the client exits.  Adding\n\
                          the option '--stay-open no' causes the server to exit\n\
                          immediately after the client exits.\n\
";

#define CLP CmdLineParser
#define CLP_SEPARATOR "!!"

static const int DEFAULT_PORT = 21590;

// Note: Changing the option name requires changing the name in Parse()
CmdLineParser::OptArgDesc Args::optArgs[] = {

  { 'V', "version",  CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL, NULL },
  { 'h', "help",     CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL, NULL },
  { 'c', "compression",  CLP::ARG_REQ,  CLP::DUPOPT_CLOB, NULL, CLP::isOptArg_long },
  { 'p', "port",     CLP::ARG_REQ,  CLP::DUPOPT_CLOB, NULL, CLP::isOptArg_long },
  { 'x', "xmlport",  CLP::ARG_REQ,  CLP::DUPOPT_CLOB, NULL, CLP::isOptArg_long },
  {  0,  "stay-open",  CLP::ARG_REQ,  CLP::DUPOPT_CLOB, NULL, NULL },

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
  mainPort = DEFAULT_PORT;  // 21590
  xmlPort = 1;  // use main port
  compression = true;
  stayOpen = true;
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
    if (parser.isOpt("help")) {
      printUsage(std::cout);
      exit(0);
    }

    if (parser.isOpt("version")) {
      printVersion(std::cout);
      exit(0);
    }

    // Check for other options: Communication options
    if (parser.isOpt("compression")) {
      const string& arg = parser.getOptArg("compression");
      compression = CmdLineParser::parseArg_bool(arg, "--compression option");
    }

    if (parser.isOpt("port")) {
      const string& arg = parser.getOptArg("port");
      mainPort = (int) CmdLineParser::toLong(arg);
      if (mainPort < 1024 && mainPort != 0)
         	  ARG_ERROR("Ports must be greater than 1024.")
    }

    if (parser.isOpt("xmlport")) {
      const string& arg = parser.getOptArg("xmlport");
      xmlPort = (int) CmdLineParser::toLong(arg);
      if (xmlPort < 1024 && xmlPort > 1)
    	   ARG_ERROR("Ports must be greater than 1024.")
    }

    if (parser.isOpt("stay-open")) {
      const string& arg = parser.getOptArg("stay-open");
      stayOpen = CmdLineParser::parseArg_bool(arg, "--stay-open option");
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
