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

//************************ System Include Files ******************************

#include <iostream>

#ifdef NO_STD_CHEADERS
# include <limits.h>
#else
# include <climits>
using namespace std; // For compatibility with non-std C headers
#endif

#include <unistd.h> /* for getopt(); cf. stdlib.h */
#include <dirent.h> 
#include <sys/types.h> 

//************************* User Include Files *******************************

#include "Args.h"
#include <lib/support/Trace.h>

//************************ Forward Declarations ******************************

using std::cerr;
using std::endl;

//****************************************************************************

int fileTrace = 0;

const String Args::HPCTOOLKIT = "HPCTOOLKIT"; 

static const char* version_info =
#include <include/HPCToolkitVersionInfo.h>

static const char* usage_summary =
"[-V] [-h dir] [-r] [-u] [-w n] [-x file [-c] [-l]] [-y file] [-t file] [-z [-o] [-f n] [-m n] [-s n.m]] configFile\n";

static const char* usage_details =
"[ GENERAL OPTIONS ]\n"
"  -V          print version information\n"
"  -h dir      Specify the destination directory all output files. The\n"
"              default is ./hpcview.output\n"
"  -r          Enable execution tracing. Use this option to debug path\n"
"              replacement if metric and program structure information is\n"
"              not being fused properly matched.\n"
"  -u          Leave trailing underscores on routine names alone. HPCView\n"
"              normally deletes any trailing underscore from routine names\n"
"              to avoid problems caused when Fortran compilers provide\n"
"              inconsistent information about routine names.\n"
"  -w n        Specify warning level. Default warning level is 0; 1 produces\n"
"              messages about creating procedure synopses\n"
"\n"
"[ OPTIONS TO GENERATE HPCVIEWER INPUT ]\n"
"  -x file     Write XML scope tree, with metrics, to 'file'. The scope tree\n"
"              can be used with HPCViewer.\n"
"  -c          Do *not* copy source code files into dataset. By default,\n"
"              hpcview -x makes copies of source files that have performance\n"
"              metrics and that can be reached by PATH/REPLACE statements\n"
"              in 'configFile'. The source files are copied to appropriate\n"
"              'viewname' directories within the (-h) output-directory. This\n"
"              results in a self-contained dataset that does not rely on an\n"
"              external source code repository.  If copying is suppressed,\n"
"              the resulting output is useful only on the original system.\n"
"  -l          By default, the generated scope tree contains aggregated\n"
"              metrics at all internal nodes of the scope tree.  This option\n"
"              saves space by outputting metrics only at the leaves. A\n"
"              FUTURE version of HPCViewer will be able to use the option,\n"
"              but no current software can.\n"
"\n"
"[ OPTIONS TO GENERATE CSV DATA FOR SPREADSHEET VIEWING]\n"
"  -y file     Write scope tree, with metrics, to 'file' in a comma \n"
"              separated value format, good for loading in a spreadsheet\n"
"              program, or for parsing with a shell script.\n"
"              Data is presented at loop level granularity.\n"
"\n"
"[ OPTIONS TO GENERATE TSV DATA FOR FLATFILE OUTPUT]\n"
"  -t file     Write scope tree, with metrics, to 'file' in a tab \n"
"              separated value format, good for loading in a gene-shaving\n"
"              program, or for parsing with a shell script.\n"
"              Data is presented at line level granularity.\n"
"\n"
"[ OPTIONS TO GENERATE STATIC HTML FOR WEB BROWSER VIEWING ]\n"
"  -z          Generate static HTML.\n"
"  -o          Generate old style HTML. Each flatten view is in a separate\n"
"              file. Good if the application is very big and the new style\n"
"              HTML files become too large.\n"
"  -f n        Compute static flattenings only for the top 'n' levels of the\n"
"              scope tree.\n" 
"  -m n        Limit the number of children reported in any scope to 'n'\n"
"              with the highest value according to the currently selected\n"
"              performance metric\n"
"  -s n.m      Suppress reporting for scopes contributing less than 'n.m'\n"
"              percent of the total cost.\n";


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
  setHPCHome(); 
  fileHome = String(hpcHome) + "/lib/html"; 

  htmlDir                = "hpcview.output"; 
  OutputInitialScopeTree = false; // used for debugging at this point
  OutputFinalScopeTree   = false;
  CopySrcFiles           = true;
  SkipHTMLfiles          = true;  // do not generate static HTML
  FlatCSVOutput          = false;
  FlatTSVOutput		 = false;
  OldStyleHTML           = false;
  XML_ToStdout           = false;
  XML_DumpAllMetrics     = true;  // dump metrics on interior nodes
  XML_Dump_File          = "scopeTree_XML.out";

  depthToFlatten = INT_MAX;
  maxLinesPerPerfPane = INT_MAX;
  deleteUnderscores = 1;
  warningLevel = 0;

  scopeThresholdPercent = THRESHOLDING_DISABLED;
}


Args::~Args()
{
}


void 
Args::PrintVersion(std::ostream& os) const
{
  cerr << cmd << ": " << version_info << endl;
}


void 
Args::PrintUsage(std::ostream& os) const
{
  cerr << "Usage: " << cmd << " " << usage_summary << endl
       << usage_details << endl;
} 


void 
Args::PrintError(std::ostream& os, const char* msg) const
{
  // FIXME: waiting until we plug the new option parser in
  os << cmd << ": " << msg << endl
     << "Try `" << cmd << " --help' for more information." << endl;
}


void
Args::Parse(int argc, const char* const argv[])
{
  // FIXME: eraxxon: drop my new argument parser in here
  cmd = argv[0]; 

  bool printVersion = false;  
  
  // -------------------------------------------------------
  // Parse the command line
  // -------------------------------------------------------
  // Note: option list follows usage message
  extern char *optarg;
  extern int optind;
  bool error = false; 
  int c;
  while ((c = getopt(argc, (char**)argv, "Vh:ruw:x:cly:t:zof:m:s:d")) != EOF) {
    switch (c) {
    
    // General Options
    case 'V': { 
      printVersion = true;
      break; 
    }
    case 'h': {
      htmlDir = optarg;
      break; 
    }
    case 'r': { 
      fileTrace++; 
      break; 
    }
    case 'u': { 
      deleteUnderscores--; 
      break; 
    }
    case 'w': {  
      warningLevel = abs(atoi(optarg));
      break; 
    }
    
    // XML output (for HPCViewer) options
    case 'x': { 
      if (! FlatCSVOutput) {
        XML_Dump_File = optarg;
        OutputFinalScopeTree = true;
        if ( XML_Dump_File == "-" ) XML_ToStdout = true;
      }
      break; 
    }
    case 'c': { // copy raw source files when generating XML 
      CopySrcFiles = false;
      break; 
    }
    case 'l': { // leaves only
      XML_DumpAllMetrics = false;
      break; 
    }

    // CSV output (for Spreadsheet) options
    case 'y': { 
      FlatCSVOutput = true;
      XML_Dump_File = optarg;
      OutputFinalScopeTree = true;
      if ( XML_Dump_File == "-" ) XML_ToStdout = true;
      break; 
    }

    // TSV output options
    case 't': { 
      FlatTSVOutput = true;
      XML_Dump_File = optarg;
      OutputFinalScopeTree = true;
      if ( XML_Dump_File == "-" ) XML_ToStdout = true;
      break; 
    }

    // Generate static HTML options
    case 'z': {  // generate static HTML
      SkipHTMLfiles = false;
      break; 
    }
    case 'o': {  // generate old style HTML
      OldStyleHTML = true;
      break; 
    }
    case 'f': { 
      depthToFlatten = atoi(optarg);
      break; 
    }
    case 'm': {
      maxLinesPerPerfPane = atoi(optarg);
      break; 
    }
    case 's': {
      scopeThresholdPercent = atof(optarg);
      break; 
    }

    // Debug and Error
    case 'd': { // debug 
      trace++; 
      break; 
    }
    case ':':
    case '?': {
      error = true; 
      break; 
    }
    }
  }

  // -------------------------------------------------------
  // Sift through results, checking for semantic errors
  // -------------------------------------------------------
  
  // Special options that should be checked first
  if (printVersion) {
    PrintVersion(cerr);
    exit(1);
  }

  error = error || (optind != argc-1); 
  if (!error) {
    configurationFile = argv[optind];
  } 

  // CopySrcFiles makes sense only if -x arg is used.
  if (false == OutputFinalScopeTree || FlatCSVOutput == true 
		  || FlatTSVOutput == true)  
    CopySrcFiles = false;

  if (error) {
    PrintUsage(cerr);
    exit(1); 
  } 
}


void 
Args::Dump(std::ostream& os) const
{
  os << "Args.cmd= " << cmd << endl; 
  os << "Args.hpcHome= " << hpcHome << endl; 
  os << "Args.fileHome= " << fileHome << endl; 
  os << "Args.htmlDir= " << htmlDir << endl; 
  os << "Args.OutputFinalScopeTree= " << OutputFinalScopeTree << endl; 
  os << "Args.OutputInitialScopeTree= " << OutputInitialScopeTree << endl; 
  os << "Args.XML_Dump_File= " << XML_Dump_File << endl; 
  os << "Args.SkipHTMLfiles= " << SkipHTMLfiles << endl; 
  os << "Args.OldStyleHTML= "  << OldStyleHTML << endl; 
  os << "Args.configurationFile= " << configurationFile << endl; 
  os << "Args.maxLinesPerPerfPane= " << maxLinesPerPerfPane << endl; 
  os << "::trace " << ::trace << endl; 
}

void 
Args::DDump() const
{
  Dump(std::cerr);
}



void 
Args::setHPCHome() 
{
  char * home = getenv(HPCTOOLKIT); 
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
  hpcHome = String(home); 
} 

