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

#include <iostream>
using std::cerr;
using std::endl;

#include <string>
using std::string;

//*************************** User Include Files ****************************

#include <include/hpctoolkit-config.h>

#include "Args.hpp"

#include <lib/support/diagnostics.h>
#include <lib/support/Trace.hpp>

//*************************** Forward Declarations **************************

//***************************************************************************

static const char* version_info = HPCTOOLKIT_VERSION_STRING;

static const char* usage_summary1 =
"[options] [<binary>] <profile>\n";

static const char* usage_summary2 =
"[options] -p [<binary>]\n";

static const char* usage_details =
"Converts various types of profile output into the PROFILE format, which in\n"
"particular, associates source file line information from <binary> with\n"
"profile data from <profile>. In effect, the output is a [source-line to\n"
"PC-profile-data] map represented as a XML scope tree (e.g. file, procedure,\n"
"statement).  Output is sent to stdout.\n"
"\n"
"To find source line information, access to the profiled binary is required.\n"
"xprof will try to find the binary from data within <profile>.  If this\n"
"information is missing, <binary> must be explicitly specified.\n"
"\n"
"By default, xprof determines a set of metrics available for the given\n" 
"profile data and includes all of them in the PROFILE output.\n"
"\n"
"The following <profile> formats are currently supported: \n"
"  - DEC/Compaq/HP's DCPI 'dcpicat' (including ProfileMe) \n"
"\n"    
"Listing available metrics:\n"
"  Note: with these options, <binary> is optional and will not be read\n"
"  -l    List all derived metrics, in compact form, available from <profile>\n"
"        and suppress generation of PROFILE output.  Note that this output\n"
"        can be used with the -M option.\n"
"  -L    List all derived metrics, in long form, available from <profile>\n"
"        and suppress generation of PROFILE output.\n"
"\n"    
"Selecting metrics:\n"
"  -M <list>, --metrics <list>\n"
"     Replace the default metric set with the colon-separated <list> and\n"
"     define the metric ordering. May be passed multiple times. Duplicates\n"
"     are allowed (though not recommended).\n"
"  -X <list>, --exclude-metrics <list>\n"
"     Exclude metrics in the colon-separated <list> from either the default\n"
"     metric set or from those specified with -M. May be passed multiple\n"
"     times.\n"
"  -R, --raw-metrics\n"
"     Generate 'raw' metrics, disabling computation of derived metrics.  For\n"
"     some profile data, such as DCPI's ProfileMe, the default is to output\n"
"     derived metrics, not the underlying raw metrics.\n"
"\n"
"General options:\n"
"  -p, --pipe     Supply <profile> on stdin.  E.g., it is often desirable to\n"
"                 pipe the output of 'dcpicat' into xprof.\n"
"  -V, --version  Print version information.\n"
"  -h, --help     Print this help.\n";


#define CLP CmdLineParser

// Note: Changing the option name requires changing the name in Parse()
CmdLineParser::OptArgDesc Args::optArgs[] = {
  // List mode
  { 'l', NULL,       CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL, NULL },
  { 'L', NULL,       CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL, NULL },

  // Normal mode
  { 'M', "metrics",         CLP::ARG_REQ , CLP::DUPOPT_CAT,  ":", NULL },
  { 'X', "exclude-metrics", CLP::ARG_REQ , CLP::DUPOPT_CAT,  ":", NULL },
  { 'R', "raw-metrics", CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL, NULL },
  {  0 , "pcmap",       CLP::ARG_REQ , CLP::DUPOPT_ERR,  NULL, NULL }, // hidden

  // General options
  { 'p', "pipe",     CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL, NULL },
  { 'V', "version",  CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL, NULL },
  { 'h', "help",     CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL, NULL },
  {  0 , "debug",    CLP::ARG_OPT,  CLP::DUPOPT_CLOB, NULL, NULL }, // hidden
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
  listAvailableMetrics = 0;
  outputRawMetrics = false;
}


Args::~Args()
{
}


void 
Args::PrintVersion(std::ostream& os) const
{
  os << getCmd() << ": " << version_info << endl;
}


void 
Args::PrintUsage(std::ostream& os) const
{
  os << "Usage: " << endl
     << getCmd() << " " << usage_summary1
     << getCmd() << " " << usage_summary2 << endl
     << usage_details << endl;
} 


void 
Args::PrintError(std::ostream& os, const char* msg) const
{
  os << getCmd() << ": " << msg << endl
     << "Try '" << getCmd() << " --help' for more information." << endl;
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
    parser.parse(optArgs, argc, argv);
    
    // -------------------------------------------------------
    // Sift through results, checking for semantic errors
    // -------------------------------------------------------
    
    // Special options that should be checked first
    trace = 0;
    if (parser.isOpt("debug")) { 
      trace = 1; 
      if (parser.isOptArg("debug")) {
	const string& arg = parser.getOptArg("debug");
	trace = (int)CmdLineParser::toLong(arg);
      }
    }
    if (parser.isOpt("help")) { 
      PrintUsage(std::cerr); 
      exit(1);
    }
    if (parser.isOpt("version")) { 
      PrintVersion(std::cerr);
      exit(1);
    }
    
    // Check for other options: List mode
    if (parser.isOpt('l')) { 
      listAvailableMetrics = 1;
    } 
    if (parser.isOpt('L')) { 
      listAvailableMetrics = 2;
    } 

    // Check for other options: normal mode
    if (parser.isOpt("metrics")) { 
      metricList = parser.getOptArg("metrics");
    }
    if (parser.isOpt("exclude-metrics")) { 
      excludeMList = parser.getOptArg("exclude-metrics");
    }
    if (parser.isOpt("raw-metrics")) { 
      outputRawMetrics = true;
    } 

    // Sanity check: -M,-X and -R should not be used at the same time
    if ( (!metricList.empty() || !excludeMList.empty()) && outputRawMetrics) {
      PrintError(std::cerr, "Error: -M or -X cannot be used with -R.\n");
      exit(1);
    }
    
    // Check for other options: General
    bool profFileFromStdin = false;
    if (parser.isOpt('p')) { 
      profFileFromStdin = true;
    } 
    
    // Check for required arguments
    string errtxt;
    int argsleft = parser.getNumArgs();
    if (profFileFromStdin) {
      switch (argsleft) {
      case 0: break; // ok
      case 1: progFile = parser.getArg(0); break;
      default: errtxt = "Too many arguments!";
      }
    } else {
      switch (argsleft) {
      case 1: profFile = parser.getArg(0); break;
      case 2: 
	progFile = parser.getArg(0); 
	profFile = parser.getArg(1); 
	break;
      default: errtxt = "Incorrect number of arguments!";
      }
    }
    
    if (!errtxt.empty()) {
      PrintError(std::cerr, errtxt);
      exit(1);
    }
    
  }
  catch (const CmdLineParser::ParseError& x) {
    PrintError(std::cerr, x.what());
    exit(1);
  }
  catch (const CmdLineParser::Exception& x) {
    DIAG_EMsg(x.message());
    exit(1);
  }
}


void 
Args::Dump(std::ostream& os) const
{
  os << "Args.cmd= " << getCmd() << endl; 
  os << "Args.progFile= " << progFile << endl;
  os << "Args.profFile= " << profFile << endl;
  os << "::trace " << ::trace << endl; 
}

void 
Args::DDump() const
{
  Dump(std::cerr);
}
