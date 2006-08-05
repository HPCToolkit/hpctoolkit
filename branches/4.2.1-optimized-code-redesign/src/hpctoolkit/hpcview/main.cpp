// -*-C++-*-
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

//************************ System Include Files ******************************

#include <iostream>
using std::cout;
using std::cerr;
using std::endl;

#include <fstream>

#include <string>
using std::string;

#include <exception>

#ifdef NO_STD_CHEADERS
# include <string.h>
#else
# include <cstring>
using namespace std; // For compatibility with non-std C headers
#endif

#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>

//************************* Xerces Include Files *******************************

#include <xercesc/util/XMLString.hpp>        
using XERCES_CPP_NAMESPACE::XMLString;

#include <xercesc/util/PlatformUtils.hpp>        
using XERCES_CPP_NAMESPACE::XMLPlatformUtils;

#include <xercesc/util/XMLException.hpp>
using XERCES_CPP_NAMESPACE::XMLException;


//************************* User Include Files *******************************

// XML related includes 
#include "PROFILEDocHandler.hpp"
#include "HPCViewDocParser.hpp"
#include "HPCViewXMLErrHandler.hpp"

// Argument Handling
#include "Args.hpp"

// ScopeInfo tree related includes 
#include "Driver.hpp"
#include "HTMLDriver.hpp"

#include <lib/prof-juicy/PgmScopeTree.hpp>

#include <lib/support/Assertion.h>
#include <lib/support/Nan.h>
#include <lib/support/diagnostics.h>
#include <lib/support/Files.hpp>
#include <lib/support/Trace.hpp>
#include <lib/support/pathfind.h>
#include <lib/support/realpath.h>

//************************ Forward Declarations ******************************

static void InitXML();
static void FiniXML();

class FileException
{
public:
  FileException(const string& s) {
    error = s;
  }
  const string& GetError() const { return error; };
private:
  string error;
};

#define NUM_PREFIX_LINES 2

static void AppendContents(std::ofstream &dest, const char *srcFile);

static string BuildConfFile(const string& hpcHome, const string& confFile);

static bool CopySourceFiles(PgmScope* pgmScopeTree, 
			    const PathTupleVec& pathVec, 
			    const string& dstDir);

//****************************************************************************

int realmain(int argc, char* const* argv);

int 
main(int argc, char* const* argv) 
{
  int ret;

  try {
    ret = realmain(argc, argv);
  } 
  catch (const Diagnostics::Exception& x) {
    cerr << "hpcview fatal error: ";
    x.report(cerr);
    exit(1);
  } 
  catch (const std::bad_alloc& x) {
    cerr << "hpcview fatal error: Memory alloc failed: " << x.what() << endl;
    exit(1);
  } 
  catch (const std::exception& x) {
    cerr << "hpcview fatal error: std::exception: " << x.what() << endl;
    exit(1);
  } 
  catch (...) {
    cerr << "hpcview fatal error: Unknown exception encountered!\n";
    exit(2);
  }

  return ret;
}

int 
realmain(int argc, char* const* argv) 
{
  InitNaN();
  Args args(argc,argv);  // exits if error on command line
  InitXML();             // exits iff failure 
  
  IFTRACE << "Initializing HTMLDriver: ..." << endl; 

  PgmScopeTree scopes("", new PgmScope("")); // name set later
  HTMLDriver htmlDriver(scopes, args.fileHome.c_str(), args.htmlDir.c_str(), args);
             // constructor exits if it can't write to htmlDir 
             // or read files in fileHome
  IFTRACE << endl; 
  
  IFTRACE << "Initializing Driver from " << args.configurationFile 
	  << ": ..." << endl; 

  //-------------------------------------------------------
  // Read configuration file
  //-------------------------------------------------------
  string tmpFile;
  try {
    tmpFile = BuildConfFile(args.hpcHome, args.configurationFile); 
    // exits iff it fails 
  }
  catch (const FileException& x) {
    cerr << x.GetError() << endl;
    exit(1);
  }

  Driver driver(args.deleteUnderscores, args.CopySrcFiles); 

  string userFile = args.configurationFile;
  HPCViewXMLErrHandler errReporter(userFile, tmpFile, NUM_PREFIX_LINES, true);
  try {
    HPCViewDocParser(driver, tmpFile, errReporter);
  }
  catch (const HPCViewDocException& x) {
    unlink(tmpFile.c_str());
    cerr << "hpcview fatal error: ";
    x.report(cerr);
    exit(1);
  }
  catch (...) {
    unlink(tmpFile.c_str()); 
    cerr << "hpcview: Fatal error processing CONFIGURATION file." << endl;
    throw;
  };

  unlink(tmpFile.c_str()); 

  //-------------------------------------------------------
  // Create metrics
  //-------------------------------------------------------
  IFTRACE << "The Driver is now: " << driver.ToString() << endl; 
  IFTRACE << "Reading the metrics: ..." << endl; 
  driver.MakePerfData(scopes); 
  IFTRACE << endl; 

  if ( args.OutputInitialScopeTree && args.OutputFinalScopeTree ) {
    cerr << "Only final scope tree (in XML) will be output" << endl;
    args.OutputInitialScopeTree = false;
  }

  if ( args.OutputInitialScopeTree ) {
    if ( args.XML_ToStdout ) { 
      cerr << "The initial scope tree (in XML) will appear on stdout" << endl; 
      driver.XML_Dump(scopes.GetRoot());
    } else {
      string dmpFile = args.htmlDir + "/" + args.XML_Dump_File;
      cerr << "The initial scope tree (in XML) will be written to " 
	   << dmpFile << endl;
      std::ofstream XML_outFile(dmpFile.c_str());
      if ( !XML_outFile ) {
	cerr << "Output file open failed; skipping write of initial scope tree"
             << endl;
      }
      driver.XML_Dump(scopes.GetRoot(), XML_outFile);
    }
  }

  //-------------------------------------------------------
  // Traverse the tree and removes all the nodes that don't have profile
  // data associated with them.
  //-------------------------------------------------------
  // do not remove the scopes with no profile data if the output is flat CSV
  if (! (args.FlatCSVOutput || args.FlatTSVOutput) )
    UpdateScopeTree( scopes.GetRoot(), driver.NumberOfMetrics() );
  
  scopes.GetRoot()->Freeze(); // disallow further additions to tree 

  scopes.CollectCrossReferences(); // collect cross referencing information

  if (trace > 1) { 
    cerr << "The final scope tree, before HTML generation:" << endl; 
    scopes.GetRoot()->Dump(); 
  }
  // FiniXML(); eraxxon: causes a seg fault.

  //-------------------------------------------------------
  // browseable database generation
  //-------------------------------------------------------
  if ( !args.SkipHTMLfiles ) {
    IFTRACE << "Writing html output to " << args.htmlDir << ". ";
    if ( args.OldStyleHTML ) {
      IFTRACE << "Generate old style HTML (flatten views in separate files): ..." 
              << endl; 
    }
    else {
      IFTRACE << "Generate new style HTML (default: flatten views of a scope are in the same files): ..." 
              << endl; 
    }
    if (htmlDriver.Write(driver) == false) {
      cerr << "ERROR: Could not generate html output." << endl; 
    }; 
    IFTRACE << endl; 
  } else {
    cerr << "HTML file generation being suppressed." << endl;
  }

  if ( args.CopySrcFiles ) {
    cerr << "Copying all source files reached by REPLACE/PATH statements to "
	 << args.htmlDir << endl;

    // Note that this may modify file names in the ScopeTree
    CopySourceFiles(scopes.GetRoot(), driver.PathVec(), args.htmlDir);
  }

  if ( args.FlatCSVOutput ) {
    if ( args.XML_ToStdout ) {
      cerr << "The final scope tree (in CSV) will appear on stdout" << endl; 
      driver.CSV_Dump(scopes.GetRoot());
    } else {
      string dmpFile = args.htmlDir + "/" + args.XML_Dump_File;
      
      cerr << "The final scope tree (in CSV) will be written to "
	   << dmpFile << endl;
      std::ofstream XML_outFile(dmpFile.c_str());
      if ( !XML_outFile ) {
	cerr << "Output file open failed; skipping write of final scope tree."
             << endl;
      }
      driver.CSV_Dump(scopes.GetRoot(), XML_outFile);
    }
  } else
  if ( args.FlatTSVOutput ) {
    if ( args.XML_ToStdout ) {
      cerr << "The final scope tree (in TSV) will appear on stdout" << endl; 
      driver.TSV_Dump(scopes.GetRoot());
    } else {
      string dmpFile = args.htmlDir + "/" + args.XML_Dump_File;
      
      cerr << "The final scope tree (in TSV) will be written to "
	   << dmpFile << endl;
      std::ofstream XML_outFile(dmpFile.c_str());
      if ( !XML_outFile ) {
	cerr << "Output file open failed; skipping write of final scope tree."
             << endl;
      }
      driver.TSV_Dump(scopes.GetRoot(), XML_outFile);
    }
  } else
  if ( args.OutputFinalScopeTree ) {
    int dumpFlags = (args.XML_DumpAllMetrics) ?
      0 : PgmScopeTree::DUMP_LEAF_METRICS;

    if ( args.XML_ToStdout ) {
      cerr << "The final scope tree (in XML) will appear on stdout" << endl; 
      driver.XML_Dump(scopes.GetRoot(), dumpFlags);
    } else {
      string dmpFile = args.htmlDir + "/" + args.XML_Dump_File;
      
      cerr << "The final scope tree (in XML) will be written to "
	   << dmpFile << endl;
      std::ofstream XML_outFile(dmpFile.c_str());
      if ( !XML_outFile ) {
	cerr << "Output file open failed; skipping write of final scope tree."
             << endl;
      }
      driver.XML_Dump(scopes.GetRoot(), dumpFlags, XML_outFile);
    }
  }
  
  ClearPerfDataSrcTable(); 
  FiniXML();
  
  return 0; 
} 

//****************************************************************************

static void 
InitXML() 
{
  IFTRACE << "Initializing XML: ..." << endl; 
  try {
    XMLPlatformUtils::Initialize();
  } 
  catch (const XMLException& toCatch) {
    cerr << "hpcview fatal error: unable to initialize XML processor." << endl 
	 << "\t" << XMLString::transcode(toCatch.getMessage()) << endl;
    exit(1); 
  }
  IFTRACE << endl; 
}

static void
FiniXML()
{
  IFTRACE << "Finalizing XML: ..." << endl; 
  XMLPlatformUtils::Terminate();
  IFTRACE << endl; 
}

#define MAX_IO_SIZE (64 * 1024)

static void 
AppendContents(std::ofstream &dest, const char *srcFile)
{
  std::ifstream src(srcFile);
  if (src.fail()) {
    string error = string("Unable to open ") + srcFile + " for reading.";
    throw FileException(error);
  }

  char buf[MAX_IO_SIZE]; 
  for(;!src.eof();) {
    src.read(buf, MAX_IO_SIZE);

    ssize_t nRead = src.gcount();
    if (nRead == 0) break;
    dest.write(buf, nRead); 
    if (dest.fail()) throw FileException("write failed");
  } 
  src.close();
}

static string
BuildConfFile(const string& hpcHome, const string& confFile) 
{
  string tmpFile = TmpFileName(); 
  string hpcloc = hpcHome;
  if (hpcloc[hpcloc.length()-1] != '/') {
    hpcloc += "/";
  }
  std::ofstream tmp(tmpFile.c_str(), std::ios_base::out);

  if (tmp.fail()) {
    string error = "Unable to open temporary file " + tmpFile + 
      " for writing.";
    throw FileException(error);
  }
  
  // the number of lines added below must equal NUM_PREFIX_LINES
  tmp << "<?xml version=\"1.0\"?>" << endl 
      << "<!DOCTYPE HPCVIEW SYSTEM \"" << hpcloc // has trailing '/'
      << "lib/dtd/HPCView.dtd\">" << endl;

  //std::cout << "TMP DTD file: '" << tmpFile << "'" << std::endl;
  //std::cout << "  " << hpcloc << std::endl;

  AppendContents(tmp, confFile.c_str());
  tmp.close();
  return tmpFile; 
}

//****************************************************************************

static int
MatchFileWithPath(const string& filenm, const PathTupleVec& pathVec);

// 'CopySourceFiles': For every file F in 'pgmScopeTree' that can be
// reached with paths in 'pathVec', copy F to its appropriate
// viewname path and update F's path to be relative to this location.
static bool
CopySourceFiles(PgmScope* pgmScopeTree, const PathTupleVec& pathVec,
		const string& dstDir)
{
  bool noError = true;

  // For each FILE
  ScopeInfoIterator it(pgmScopeTree, &ScopeTypeFilter[ScopeInfo::FILE]); 
  for (; it.Current(); /* */) {
    FileScope* file = dynamic_cast<FileScope*>(it.Current());
    BriefAssertion(file != NULL);
    it++; // advance iterator -- it is pointing at 'file'
    
    // Note: 'fileOrig' will be not be absolute if it is not possible to find
    // the file on the current filesystem. (cf. NodeRetriever::MoveToFile)
    string fileOrig = file->Name();
    if (fileOrig[0] == '/') {
      int indx = MatchFileWithPath(fileOrig, pathVec);
      if (indx >= 0) {
	const string& path     = pathVec[indx].first;
	const string& viewname = pathVec[indx].second;
	
	// find the absolute form of 'path'
	string realpath(path);
	if (is_recursive_path(realpath.c_str())) {
	  realpath[realpath.length()-RECURSIVE_PATH_SUFFIX_LN] = '\0';
	}
	realpath = RealPath(realpath.c_str()); 
	
	// 'realpath' must be a prefix of 'fileOrig'; find left-over portion
	char* pathSuffx = const_cast<char*>(fileOrig.c_str());
	pathSuffx = &pathSuffx[realpath.length()];
	while (pathSuffx[0] != '/') { --pathSuffx; } // should start with '/'
	
	// Create new file name and copy commands
	string newSIFileNm = "./" + viewname + pathSuffx;
	string toFile;
	if (dstDir[0]  != '/') {
	  toFile = "./";
	}
	toFile = toFile + dstDir + "/" + viewname + pathSuffx;
	string toDir(toFile); // need to strip off ending filename to 
	unsigned int end;     // get full path for 'toFile'
	for (end = toDir.length() - 1; toDir[end] != '/'; end--) { }
	toDir[end] = '\0'; // should not end with '/'
	
	string cmdMkdir = "mkdir -p " + toDir;
	string cmdCp    = "cp -f " + fileOrig + " " + toFile;
	//cerr << cmdCp << endl;
	
	// could use CopyFile; see StaticFiles::Copy
	if (system(cmdMkdir.c_str()) == 0 && system(cmdCp.c_str()) == 0) {
	  cerr << "  " << toFile << endl;
	  file->SetName(newSIFileNm);
	} else {
	  cerr << "ERROR copying: '" << toFile << "'\n";
	}
      }
    } 
  }
  
  return noError;
}

// 'MatchFileWithPath': use 'pathfind_r' to determine which path in
// 'pathVec', if any, reaches 'filenm'.  If a match is found, returns
// an index in pathVec; otherwise returns a negative.
static int
MatchFileWithPath(const string& filenm, const PathTupleVec& pathVec)
{
  // Find the index to the path that reaches 'filenm'.
  // It is possible that more than one path could reach the same
  //   file because of substrings.
  //   E.g.: p1: /p1/p2/p3/*, p2: /p1/p2/*, f: /p1/p2/p3/f.c
  //   Choose the path that is most qualified (We assume RealPath length
  //   is valid test.) 
  int foundIndex = -1; // index into 'pathVec'
  int foundPathLn = 0; // length of the path represented by 'foundIndex'
  for (unsigned int i = 0; i < pathVec.size(); i++) {
    // find the absolute form of 'curPath'
    const string& curPath = pathVec[i].first;
    string realPath(curPath);
    if (is_recursive_path(curPath.c_str())) {
      realPath[realPath.length()-RECURSIVE_PATH_SUFFIX_LN] = '\0';
    }
    realPath = RealPath(realPath.c_str());
    int realPathLn = realPath.length();
       
    // 'filenm' should be relative as input for pathfind_r.  If 'filenm'
    // is absolute and 'realPath' is a prefix, make it relative. 
    string tmpFile(filenm);
    char* curFile = const_cast<char*>(tmpFile.c_str());
    if (filenm[0] == '/') { // is 'filenm' absolute?
      if (strncmp(curFile, realPath.c_str(), realPathLn) == 0) {
	curFile = &curFile[realPathLn];
	while (curFile[0] == '/') { ++curFile; } // should not start with '/'
      } 
      else {
	continue; // pathfind_r can't posibly find anything
      }
    }
    
    const char* result = pathfind_r(curPath.c_str(), curFile, "r");
    if (result) {
      bool update = false;
      if (foundIndex < 0) {
	update = true;
      } else if ((foundIndex >= 0) && (realPathLn > foundPathLn)) {
	update = true;
      }
      
      if (update) {
	foundIndex = i;
	foundPathLn = realPathLn;
      }
    }
  }
  return foundIndex;
}

