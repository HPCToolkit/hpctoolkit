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
#include <fstream> 

#ifdef NO_STD_CHEADERS
# include <string.h>
#else
# include <cstring>
using namespace std; // For compatibility with non-std C headers
#endif

#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>

//************************* User Include Files *******************************

// XML related includes 
#include "XMLAdapter.h" 
#include "PROFILEDocHandler.h" 
#include "HPCViewDocParser.h"

// Argument Handling
#include "Args.h" 

// ScopeInfo tree related includes 
#include "Driver.h" 
#include "ScopesInfo.h" 
#include "HTMLDriver.h"

#include <lib/support/String.h>
#include <lib/support/Assertion.h>
#include <lib/support/Nan.h>
#include <lib/support/Files.h>
#include <lib/support/Trace.h>
#include <lib/support/pathfind.h>
#include <lib/support/realpath.h>

//************************ Forward Declarations ******************************

using std::cout;
using std::cerr;
using std::endl;

static void InitXML();
static void FiniXML();

class FileException
{
public:
  FileException(String s) {
    error = s;
  }
  String GetError() const { return error; };
private:
  String error;
};

#define NUM_PREFIX_LINES 2

static String ConfigDtd(const char* hpcHome);

static void AppendContents(std::ofstream &dest, const char *srcFile);

static String BuildConfFile(const char* hpcHome, const char* confFile);

static bool CopySourceFiles(PgmScope* pgmScopeTree, 
			    const PathTupleVec& pathVec, 
			    const char* dstDir);

//****************************************************************************

int realmain(int argc, char* const* argv);

int 
main(int argc, char* const* argv) 
{
  int ret;

  try {
    ret = realmain(argc, argv);
  } catch (std::bad_alloc& x) {
    cerr << "Error: Memory alloc failed!\n";
    exit(1);
  } catch (...) {
    cerr << "Error: Unknown exception encountered!\n";
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
  ScopesInfo scopes(""); // name will be set as the scope tree is built
  HTMLDriver htmlDriver(scopes, args.fileHome, args.htmlDir, args); 
             // constructor exits if it can't write to htmlDir 
             // or read files in fileHome
  IFTRACE << endl; 
  
  IFTRACE << "Initializing Driver from " << args.configurationFile 
	  << ": ..." << endl; 

  //-------------------------------------------------------
  // Read configuaration file
  //-------------------------------------------------------
  String tmpFile;
  try {
    tmpFile = BuildConfFile(args.hpcHome, args.configurationFile); 
    // exits iff it fails 
  }
  catch (const FileException &e) {
    cerr << e.GetError() << endl;
    exit(-1);
  }

  Driver driver(args.deleteUnderscores, args.CopySrcFiles); 

  String userFile(args.configurationFile);
  DOMTreeErrorReporter errReporter(userFile, tmpFile, NUM_PREFIX_LINES);

  HPCViewDocParser(driver, tmpFile, errReporter);
  unlink(tmpFile); 

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
      driver.XML_Dump(scopes.Root());
    } else {
      String dmpFile = args.htmlDir + "/" + args.XML_Dump_File;
      cerr << "The initial scope tree (in XML) will be written to " 
	   << dmpFile << endl;
      std::ofstream XML_outFile(dmpFile);
      if ( !XML_outFile ) {
	cerr << "Output file open failed; skipping write of initial scope tree"
             << endl;
      }
      driver.XML_Dump(scopes.Root(), XML_outFile);
    }
  }

  //-------------------------------------------------------
  // Traverse the tree and removes all the nodes that don't have profile
  // data associated with them.
  //-------------------------------------------------------
  UpdateScopeTree( scopes.Root(), driver.NumberOfMetrics() );
  
  scopes.Root()->Freeze(); // disallow further additions to tree 

  scopes.CollectCrossReferences(); // collect cross referencing information

  if (trace > 1) { 
    cerr << "The final scope tree, before HTML generation:" << endl; 
    scopes.Root()->Dump(); 
  }
  // FiniXML(); eraxxon: causes a seg fault.

  //-------------------------------------------------------
  // browseable database generation
  //-------------------------------------------------------
  if ( !args.SkipHTMLfiles ) {
    IFTRACE << "Writing html output to " << args.htmlDir << ": ..." << endl; 
    if (htmlDriver.Write(driver) == false) {
      cerr << "ERROR: Could not generate html output." << endl; 
    }; 
    IFTRACE << endl; 
  } else {
    cerr << "HTML file generation being suppressed." << endl;
  }

  if ( args.CopySrcFiles ) {
    cerr << "Copying all source files reached by REPLACE/PATH statments to "
	 << args.htmlDir << endl;

    // Note that this may modify file names in the ScopeTree
    CopySourceFiles(scopes.Root(), driver.PathVec(), args.htmlDir);
  }

  if ( args.OutputFinalScopeTree ) {
    int dumpFlags = (args.XML_DumpAllMetrics) ?
      0 : ScopeInfo::DUMP_LEAF_METRICS;

    if ( args.XML_ToStdout ) {
      cerr << "The final scope tree (in XML) will appear on stdout" << endl; 
      driver.XML_Dump(scopes.Root(), dumpFlags);
    } else {
      String dmpFile = args.htmlDir + "/" + args.XML_Dump_File;
      
      cerr << "The final scope tree (in XML) will be written to "
	   << dmpFile << endl;
      std::ofstream XML_outFile(dmpFile);
      if ( !XML_outFile ) {
	cerr << "Output file open failed; skipping write of final scope tree."
             << endl;
      }
      driver.XML_Dump(scopes.Root(), dumpFlags, XML_outFile);
    }
  }
  
  ClearPerfDataSrcTable(); 
  
  return 0; 
} 

//****************************************************************************

static void 
InitXML() 
{
  IFTRACE << "Initializing XML: ..." << endl; 
  // Initialize the XML4C2 system
  try {
    XMLPlatformUtils::Initialize();
  } 
  catch (const XMLException& toCatch) {
    cerr << "ERROR during initialization! :\n" 
	 << XMLStrToString(toCatch.getMessage())
	 << endl;
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

static String
ConfigDtd(const char* hpcHome) 
{
  return String(hpcHome) + "/lib/dtd/HPCView.dtd"; 
} 

#define MAX_IO_SIZE (64 * 1024)

static void 
AppendContents(std::ofstream &dest, const char *srcFile)
{
  std::ifstream src(srcFile);
  if (src.fail()) {
    String error = String("Unable to open ") + srcFile + " for reading.";
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

static String
BuildConfFile(const char* hpcHome, const char* confFile) 
{
  String tmpFile = TmpFileName(); 
  String hpcloc = String(hpcHome);
  if (hpcloc[hpcloc.Length()-1] != '/') {
    hpcloc += "/";
  }
  std::ofstream tmp(tmpFile);

  if (tmp.fail()) {
    String error = String("Unable to open temporary file ") + tmpFile + 
      " for writing.";
    throw FileException(error);
  }
  
  // the number of lines added below must equal NUM_PREFIX_LINES
  tmp << "<?xml version=\"1.0\"?>" << endl 
      << "<!DOCTYPE HPCVIEW SYSTEM \"" << hpcloc // has trailing '/'
      << "lib/dtd/HPCView.dtd\">" << endl;

  AppendContents(tmp,confFile);
  tmp.close();
  return tmpFile; 
}

//****************************************************************************

static int
MatchFileWithPath(const char* filenm, const PathTupleVec& pathVec);

// 'CopySourceFiles': For every file F in 'pgmScopeTree' that can be
// reached with paths in 'pathVec', copy F to its appropriate
// viewname path and update F's path to be relative to this location.
static bool
CopySourceFiles(PgmScope* pgmScopeTree, const PathTupleVec& pathVec,
		const char* dstDir)
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
    String fileOrig(file->Name());
    if (fileOrig[(unsigned int)0] == '/') { 
      int indx = MatchFileWithPath(fileOrig, pathVec);
      if (indx >= 0) {
	const String& path     = pathVec[indx].first;
	const String& viewname = pathVec[indx].second;
	
	// find the absolute form of 'path'
	String realpath(path);
	if (is_recursive_path(realpath)) {
	  realpath[realpath.Length()-RECURSIVE_PATH_SUFFIX_LN] = '\0';
	}
	realpath = RealPath(realpath); 
	
	// 'realpath' must be a prefix of 'fileOrig'; find left-over portion
	char* pathSuffx = const_cast<char*>((const char*)fileOrig);
	pathSuffx = &pathSuffx[strlen(realpath)];
	while (pathSuffx[0] != '/') { --pathSuffx; } // should start with '/'
	
	// Create new file name and copy commands
	String newSIFileNm = "./" + viewname + pathSuffx;
	String toFile ;
	if (dstDir[0]  != '/') {
	  toFile = String("./");
	}
	toFile = toFile + dstDir + "/" + viewname + pathSuffx;
	String toDir(toFile); // need to strip off ending filename to 
	unsigned int end;     // get full path for 'toFile'
	for (end = toDir.Length() - 1; toDir[end] != '/'; end--) { }
	toDir[end] = '\0'; // should not end with '/'
	
	String cmdMkdir = "mkdir -p " + toDir;
	String cmdCp    = "cp -f " + fileOrig + " " + toFile;
	//cerr << cmdCp << endl;
	
	// could use CopyFile; see StaticFiles::Copy
	if (system(cmdMkdir) == 0 && system(cmdCp) == 0) {
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
MatchFileWithPath(const char* filenm, const PathTupleVec& pathVec)
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
    const char* curPath = pathVec[i].first;
    String realPath(curPath);
    if (is_recursive_path(curPath)) {
      realPath[realPath.Length()-RECURSIVE_PATH_SUFFIX_LN] = '\0';
    }
    realPath = RealPath(realPath);
    int realPathLn = realPath.Length();
       
    // 'filenm' should be relative as input for pathfind_r.  If 'filenm'
    // is absolute and 'realPath' is a prefix, make it relative. 
    String tmpFile(filenm);
    char* curFile = const_cast<char*>((const char*)tmpFile);
    if (filenm[0] == '/') { // is 'filenm' absolute?
      if (strncmp(curFile, realPath, realPathLn) == 0) {
	curFile = &curFile[realPathLn];
	while (curFile[0] == '/') { ++curFile; } // should not start with '/'
      } else {
	continue; // pathfind_r can't posibly find anything
      }
    }
    
    char* result = pathfind_r(curPath, curFile, "r");
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

