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

//************************ System Include Files ******************************

#include <iostream>
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

//*********************** Xerces Include Files *******************************

//************************* User Include Files *******************************

#include "Args.hpp"
#include "Driver.hpp"

#include "HPCViewDocParser.hpp"

#include <lib/prof-juicy-x/XercesUtil.hpp>
#include <lib/prof-juicy-x/XercesErrorHandler.hpp>

#include <lib/prof-juicy/PgmScopeTree.hpp>

#include <lib/support/diagnostics.h>
#include <lib/support/Nan.h>
#include <lib/support/Files.hpp>
#include <lib/support/IOUtil.hpp>
#include <lib/support/pathfind.h>
#include <lib/support/realpath.h>

//************************ Forward Declarations ******************************

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
    DIAG_EMsg(x.message());
    exit(1);
  } 
  catch (const std::bad_alloc& x) {
    DIAG_EMsg("[std::bad_alloc] " << x.what());
    exit(1);
  } 
  catch (const std::exception& x) {
    DIAG_EMsg("[std::exception] " << x.what());
    exit(1);
  }
  catch (...) {
    DIAG_EMsg("Unknown exception encountered!");
    exit(2);
  }

  return ret;
}


int 
realmain(int argc, char* const* argv) 
{
  InitNaN();
  Args args(argc,argv);  // exits if error on command line
  InitXerces();          // exits iff failure 

  // FIXME: pulled out of HTMLDriver
  if (MakeDir(args.dbDir.c_str()) != 0) {
    exit(1);
  }

  Driver driver(args.deleteUnderscores, args.CopySrcFiles); 

  PgmScopeTree scopeTree("", new PgmScope("")); // FIXME: better location
  
  //-------------------------------------------------------
  // 1. Read configuration file
  //-------------------------------------------------------
  const string& cfgFile = args.configurationFile;
  DIAG_Msg(3, "Initializing Driver from " << cfgFile); 
  
  string tmpFile;
  try {
    tmpFile = BuildConfFile(args.hpcHome, cfgFile); 
    // exits iff it fails 
  }
  catch (const FileException& x) {
    DIAG_EMsg(x.GetError());
    exit(1);
  }
  
  try {
    XercesErrorHandler errHndlr(cfgFile, tmpFile, NUM_PREFIX_LINES, true);
    HPCViewDocParser parser(tmpFile, errHndlr);
    parser.pass1(driver); // FIXME: merge into one pass
    parser.pass2(driver);
  }
  catch (const SAXParseException& x) {
    unlink(tmpFile.c_str());
    //DIAG_EMsg(XMLString::transcode(x.getMessage()));
    exit(1);
  }
  catch (const HPCViewDocException& x) {
    unlink(tmpFile.c_str());
    DIAG_EMsg(x.message());
    exit(1);
  }
  catch (...) {
    unlink(tmpFile.c_str());
    DIAG_EMsg("While processing '" << cfgFile << "'...");
    throw;
  };

  unlink(tmpFile.c_str());
  DIAG_Msg(3, "Driver is now: " << driver.ToString());
  
  
  //-------------------------------------------------------
  // 2. Initialize scope tree
  //-------------------------------------------------------
  DIAG_Msg(3, "Initializing scope tree...");
  driver.ScopeTreeInitialize(scopeTree); 

  //-------------------------------------------------------
  // 3. Correlate program source with metrics
  //-------------------------------------------------------
  DIAG_Msg(3, "Creating and correlating metrics with program structure: ...");
  driver.ScopeTreeComputeMetrics(scopeTree);

  DIAG_If(4) {
    DIAG_Msg(4, "Initial scope tree:");
    int flg = (args.XML_DumpAllMetrics) ? 0 : PgmScopeTree::DUMP_LEAF_METRICS;
    driver.XML_Dump(scopeTree.GetRoot(), flg, std::cerr);
  }
  
  //-------------------------------------------------------
  // 4. Finalize scope tree
  //-------------------------------------------------------
  if (args.OutFilename_CSV.empty() && args.OutFilename_TSV.empty()) {
    // Prune the scope tree (remove scopeTree without metrics)
    PruneScopeTreeMetrics(scopeTree.GetRoot(), driver.NumberOfMetrics());
  }
  
  scopeTree.GetRoot()->Freeze();      // disallow further additions to tree 
  scopeTree.CollectCrossReferences(); // collect cross referencing information

  FiniXerces();

  DIAG_If(4) {
    DIAG_Msg(4, "Final scope tree:");
    int flg = (args.XML_DumpAllMetrics) ? 0 : PgmScopeTree::DUMP_LEAF_METRICS;
    driver.XML_Dump(scopeTree.GetRoot(), flg, std::cerr);
  }

  //-------------------------------------------------------
  // 5. Generate Experiment database
  //-------------------------------------------------------
  if (args.CopySrcFiles) {
    DIAG_Msg(1, "Copying source files reached by REPLACE/PATH statements to " << args.dbDir);
    
    // Note that this may modify file names in the ScopeTree
    CopySourceFiles(scopeTree.GetRoot(), driver.PathVec(), args.dbDir);
  }

  if (!args.OutFilename_CSV.empty()) {
    const string& fnm = args.OutFilename_CSV;
    DIAG_Msg(1, "Writing final scope tree (in CSV) to " << fnm);
    string fpath = args.dbDir + "/" + fnm;
    const char* osnm = (fnm == "-") ? NULL : fpath.c_str();
    std::ostream* os = IOUtil::OpenOStream(osnm);
    driver.CSV_Dump(scopeTree.GetRoot(), *os);
    IOUtil::CloseStream(os);
  } 

  if (!args.OutFilename_TSV.empty()) {
    const string& fnm = args.OutFilename_TSV;
    DIAG_Msg(1, "Writing final scope tree (in TSV) to " << fnm);
    string fpath = args.dbDir + "/" + fnm;
    const char* osnm = (fnm == "-") ? NULL : fpath.c_str();
    std::ostream* os = IOUtil::OpenOStream(osnm);
    driver.TSV_Dump(scopeTree.GetRoot(), *os);
    IOUtil::CloseStream(os);
  }
  
  if (args.OutFilename_XML != "no") {
    int flg = (args.XML_DumpAllMetrics) ? 0 : PgmScopeTree::DUMP_LEAF_METRICS;

    const string& fnm = args.OutFilename_XML;
    DIAG_Msg(1, "Writing final scope tree (in XML) to " << fnm);
    string fpath = args.dbDir + "/" + fnm;
    const char* osnm = (fnm == "-") ? NULL : fpath.c_str();
    std::ostream* os = IOUtil::OpenOStream(osnm);
    driver.XML_Dump(scopeTree.GetRoot(), flg, *os);
    IOUtil::CloseStream(os);
  }

  //-------------------------------------------------------
  // Cleanup
  //-------------------------------------------------------
  ClearPerfDataSrcTable(); 
  
  return 0; 
} 


//****************************************************************************


static void 
AppendContents(std::ofstream &dest, const char *srcFile)
{
#define MAX_IO_SIZE (64 * 1024)

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
      << "share/hpctoolkit/dtd/HPCView.dtd\">" << endl;

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
    DIAG_Assert(file != NULL, "");
    it++; // advance iterator -- it is pointing at 'file'
    
    // Note: 'fileOrig' will be not be absolute if it is not possible to find
    // the file on the current filesystem. (cf. NodeRetriever::MoveToFile)
    string fileOrig = file->name();
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
	  DIAG_Msg(1, "  " << toFile);
	  file->SetName(newSIFileNm);
	} 
	else {
	  DIAG_EMsg("copying: '" << toFile);
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

