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

//*************************** User Include Files ****************************

#include "Args.hpp"
#include <lib/support/Trace.hpp>

//*************************** Forward Declarations **************************

using std::cerr;
using std::endl;
using std::string;

//***************************************************************************

static const char* version_info =
#include <include/HPCToolkitVersionInfo.h>

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

#if 0  // FIXME '[-m <bloop-pcmap>]'
"If no <bloop-pcmap> -- a map extended with analysis information\n"
"from 'bloop' -- is provided, the program attempts to construct\n"
"the PROFILE by querying the <binary>'s debugging information.\n"
"Because of the better analysis ability of 'bloop', a <bloop-pcmap>\n"
"usually improves the accuracy of the PROFILE.  Moreover, because\n"
"no loop recovery is performed, providing <bloop-pcmap> enables\n"
"the PROFILE to represent loop nesting information.\n"
"[*Not fully implemented.*]\n"
"\n"
"  -m: specify <bloop-pcmap>\n"
#endif     


#define CLP CmdLineParser

// Note: Changing the option name requires changing the name in Parse()
CmdLineParser::OptArgDesc Args::optArgs[] = {
  // List mode
  { 'l', NULL,       CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL },
  { 'L', NULL,       CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL },

  // Normal mode
  { 'M', "metrics",         CLP::ARG_REQ , CLP::DUPOPT_CAT,  ":" },
  { 'X', "exclude-metrics", CLP::ARG_REQ , CLP::DUPOPT_CAT,  ":" },
  { 'R', "raw-metrics", CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL },
  {  0 , "pcmap",       CLP::ARG_REQ , CLP::DUPOPT_ERR,  NULL }, // hidden

  // General options
  { 'p', "pipe",     CLP::ARG_NONE, CLP::DUPOPT_CLOB, NULL },
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
  listAvailableMetrics = 0;
  outputRawMetrics = false;
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
  os << "Usage: " << endl
     << GetCmd() << " " << usage_summary1
     << GetCmd() << " " << usage_summary2 << endl
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
    trace = 0;
    if (parser.IsOpt("debug")) { 
      trace = 1; 
      if (parser.IsOptArg("debug")) {
	const string& arg = parser.GetOptArg("debug");
	trace = (int)CmdLineParser::ToLong(arg);
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
    
    // Check for other options: List mode
    if (parser.IsOpt('l')) { 
      listAvailableMetrics = 1;
    } 
    if (parser.IsOpt('L')) { 
      listAvailableMetrics = 2;
    } 

    // Check for other options: normal mode
    if (parser.IsOpt("metrics")) { 
      metricList = parser.GetOptArg("metrics");
    }
    if (parser.IsOpt("exclude-metrics")) { 
      excludeMList = parser.GetOptArg("exclude-metrics");
    }
    if (parser.IsOpt("raw-metrics")) { 
      outputRawMetrics = true;
    } 
    if (parser.IsOpt("pcmap")) { 
      pcMapFile = parser.GetOptArg("pcmap");
    }

    // Sanity check: -M,-X and -R should not be used at the same time
    if ( (!metricList.empty() || !excludeMList.empty()) && outputRawMetrics) {
      PrintError(std::cerr, "Error: -M or -X cannot be used with -R.\n");
      exit(1);
    }
    
    // Check for other options: General
    bool profFileFromStdin = false;
    if (parser.IsOpt('p')) { 
      profFileFromStdin = true;
    } 
    
    // Check for required arguments
    string errtxt;
    int argsleft = parser.GetNumArgs();
    if (profFileFromStdin) {
      switch (argsleft) {
      case 0: break; // ok
      case 1: progFile = parser.GetArg(0); break;
      default: errtxt = "Too many arguments!";
      }
    } else {
      switch (argsleft) {
      case 1: profFile = parser.GetArg(0); break;
      case 2: 
	progFile = parser.GetArg(0); 
	profFile = parser.GetArg(1); 
	break;
      default: errtxt = "Incorrect number of arguments!";
      }
    }
    
    if (!errtxt.empty()) {
      PrintError(std::cerr, errtxt);
      exit(1);
    }
    
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
  os << "Args.pcMapFile= " << pcMapFile << endl;
  os << "Args.progFile= " << progFile << endl;
  os << "Args.profFile= " << profFile << endl;
  os << "::trace " << ::trace << endl; 
}

void 
Args::DDump() const
{
  Dump(std::cerr);
}
