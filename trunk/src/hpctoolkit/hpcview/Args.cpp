// $Id$
// -*-C++-*-
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

const String Args::HPCTOOLKIT = "HPCTOOLKIT"; 


const char* hpctoolkitVerInfo=
#include <include/HPCToolkitVersionInfo.h>

void Args::Version()
{
  cerr << cmd << ": " << hpctoolkitVerInfo << endl;
}

void Args::Usage()
{
  cerr
  << "Usage:\n"
  << "       " << cmd << " [-V] [-h dir] [-r] [-u] [-w n] [-x file [-c] [-l]]  [-z [-f n] [-m n] [-s n.m]] configFile" << endl
  << "\n"
  << " [ GENERAL OPTIONS ]\n"
  << "     -V       print version information\n"
  << "     -h dir   Specify the destination directory all output files.\n"
  << "              The default is ./hpcview.output\n"
  << "     -r       Enable execution tracing. Use this option to debug path\n"
  << "              replacement if metric and program structure information\n"
  << "              is not being fused properly matched.\n"
  << "     -u       Leave trailing underscores on routine names alone.\n"
  << "              HPCView normally deletes any trailing underscore from\n"
  << "              routine names to avoid problems caused when Fortran\n"
  << "              compilers provide inconsistent information about routine\n"
  << "              names.\n"
  << "     -w n     Specify warning level.  Default warning level is 0; \n"
  << "              1 produces messages about creating procedure synopses\n"
  << "\n"
  << " [ OPTIONS TO GENERATE HPCVIEWER INPUT ]\n"
  << "     -x file  Write XML scope tree, with metrics, to 'file'. The scope\n"
  << "              tree can be used with the HPCViewer.\n"
  << "     -c       By default, hpcview -x makes copies of source files that\n"
  << "              have performance metrics and that can be reached by PATH\n"
  << "              and REPLACE statements in 'configFile'.  The files are\n"
  << "              copied to appropriate 'viewname' directories within the\n"
  << "              (-h) output-directory.\n"
  << "              This results in a self-contained dataset that can viewed\n"
  << "              in contexts, e.g., after copying to a new system, that\n"
  << "              do not have access to the source in its current position\n"
  << "              in the file system.  Setting -c suppresses copying\n"
  << "              The resulting output is useful only on the original\n"
  << "              system.\n"
  << "     -l       By default, the scope tree generated above contains\n"
  << "              aggregated metrics at all internal nodes of the scope\n"
  << "              tree.  The -l option saves space by outputting metrics\n"
  << "              only at the leaves.  A FUTURE version of HPCViewer will\n"
  << "              be able to use option, but no current software can.\n"
  << "\n"
  << " [ OPTIONS TO GENERATE STATIC HTML FOR WEB BROWSER VIEWING ]\n"
  << "     -z       Generate static HTML.\n"
  << "     -f n     Compute static flattenings only for the top 'n' levels\n"
  << "              of the scope tree.\n" 
  << "     -m n     Limit the number of children reported in any scope to \n"
  << "              the 'n' with the highest value according to the \n"
  << "              currently selected performance metric\n"
  << "     -s n.m   Suppress reporting for scopes contributing less than\n"
  << "              'n.m' percent of the total cost.\n"
  << "\n";
} 

int fileTrace = 0;
  
Args::Args(int argc, char* const* argv) {
  cmd = argv[0]; 
  setHPCHome(); 
  fileHome = String(hpcHome) + "/lib/html"; 
  
  extern char *optarg;
  extern int optind;
  deleteUnderscores=1;
  warningLevel=0;
  htmlDir = "hpcview.output"; 

  bool printVersion = false;  

  OutputInitialScopeTree = false; // used for debugging at this point
  OutputFinalScopeTree   = false;
  CopySrcFiles           = true;
  SkipHTMLfiles          = true;  // do not generate static HTML
  XML_ToStdout           = false;
  XML_DumpAllMetrics     = true;  // dump metrics on interior nodes
  XML_Dump_File          = "scopeTree_XML.out";

  maxLinesPerPerfPane = INT_MAX;
  depthToFlatten = INT_MAX;
  scopeThresholdPercent = THRESHOLDING_DISABLED;
  bool error = false; 

  // Note: option list follows usage message
  int c;
  while ((c = getopt(argc, argv, "Vh:ruw:x:clzf:m:s:d")) != EOF) {
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
      XML_Dump_File = optarg;
      OutputFinalScopeTree = true;
      if ( XML_Dump_File == "-" ) XML_ToStdout = true;
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

    // Generate static HTML options
    case 'z': {  // generate static HTML
      SkipHTMLfiles = false;
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
  error = error || (optind != argc-1); 
  if (!error) {
    configurationFile = argv[optind];
  } 

  // CopySrcFiles makes sense only if -x arg is used.
  if (false == OutputFinalScopeTree)  CopySrcFiles = false;

  IFTRACE << "Args.cmd= " << cmd << endl; 
  IFTRACE << "Args.hpcHome= " << hpcHome << endl; 
  IFTRACE << "Args.fileHome= " << fileHome << endl; 
  IFTRACE << "Args.htmlDir= " << htmlDir << endl; 
  IFTRACE << "Args.OutputFinalScopeTree= " << OutputFinalScopeTree << endl; 
  IFTRACE << "Args.OutputInitialScopeTree= " << OutputInitialScopeTree << endl; 
  IFTRACE << "Args.XML_Dump_File= " << XML_Dump_File << endl; 
  IFTRACE << "Args.SkipHTMLfiles= " << SkipHTMLfiles << endl; 
  IFTRACE << "Args.configurationFile= " << configurationFile << endl; 
  IFTRACE << "Args.maxLinesPerPerfPane= " << maxLinesPerPerfPane << endl; 
  IFTRACE << "::trace " << ::trace << endl; 

  if (printVersion) {
    Version();
    exit(1);
  }

  if (error) {
    Usage(); 
    exit(1); 
  } 
}


void Args::setHPCHome() 
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

