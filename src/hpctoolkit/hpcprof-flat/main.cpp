// -*-Mode: C++;-*-
// $Id$

// * BeginRiceCopyright *****************************************************
// 
// Copyright ((c)) 2002-2007, Rice University 
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

#include <string>
using std::string;

#include <exception>

#ifdef NO_STD_CHEADERS
# include <string.h>
#else
# include <cstring>
using namespace std; // For compatibility with non-std C headers
#endif

#include <set>

//*********************** Xerces Include Files *******************************

//************************* User Include Files *******************************

#include "Args.hpp"
#include "ConfigParser.hpp"

#include <lib/analysis/Flat_SrcCorrelation.hpp>

#include <lib/prof-juicy-x/PGMReader.hpp>
#include <lib/prof-juicy-x/XercesUtil.hpp>
#include <lib/prof-juicy-x/XercesErrorHandler.hpp>

#include <lib/prof-juicy/MetricDescMgr.hpp>
#include <lib/prof-juicy/PgmScopeTree.hpp>

#include <lib/support/diagnostics.h>
#include <lib/support/Files.hpp>
#include <lib/support/IOUtil.hpp>
#include <lib/support/NaN.h>
#include <lib/support/pathfind.h>
#include <lib/support/realpath.h>

//************************ Forward Declarations ******************************

static void
readConfFile(Args& args, Prof::MetricDescMgr& metricMgr);

static bool 
copySourceFiles(PgmScope* pgmScopeTree, 
		const Analysis::PathTupleVec& pathVec, 
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
  Args args(argc, argv);  // exits if error on command line

  NaN_init();

  //-------------------------------------------------------
  // Create metric descriptors
  //-------------------------------------------------------
  Prof::MetricDescMgr metricMgr;

  DIAG_Msg(2, "Creating metrics... ");
  if (args.configurationFileMode) {
    readConfFile(args, metricMgr); // exits on failure
  }
  else {
    metricMgr.makeTable(args.profileFiles);
  }
  

  Analysis::Flat::Driver driver(args, metricMgr);

  //-------------------------------------------------------
  // 1. Initialize static program structure
  //-------------------------------------------------------
  DIAG_Msg(2, "Initializing scope tree...");
  PgmScopeTree scopeTree("", new PgmScope("")); // FIXME: better location
  driver.createProgramStructure(scopeTree); 

  //-------------------------------------------------------
  // 2. Correlate metrics with program structure
  //-------------------------------------------------------
  DIAG_Msg(2, "Creating and correlating metrics with program structure: ...");
  driver.correlateMetricsWithStructure(scopeTree);

  DIAG_If(3) {
    DIAG_Msg(3, "Initial scope tree:");
    int flg = (args.metrics_computeInteriorValues) ? 0 : PgmScopeTree::DUMP_LEAF_METRICS;
    driver.XML_Dump(scopeTree.GetRoot(), flg, std::cerr);
  }
  
  //-------------------------------------------------------
  // 3. Finalize scope tree
  //-------------------------------------------------------
  if (args.outFilename_CSV.empty() && args.outFilename_TSV.empty()) {
    // Prune the scope tree (remove scopeTree without metrics)
    PruneScopeTreeMetrics(scopeTree.GetRoot(), metricMgr.size());
  }
  
  scopeTree.GetRoot()->Freeze();      // disallow further additions to tree 
  scopeTree.CollectCrossReferences(); // collect cross referencing information


  DIAG_If(3) {
    DIAG_Msg(3, "Final scope tree:");
    int flg = (args.metrics_computeInteriorValues) ? 0 : PgmScopeTree::DUMP_LEAF_METRICS;
    driver.XML_Dump(scopeTree.GetRoot(), flg, std::cerr);
  }

  //-------------------------------------------------------
  // 4. Generate Experiment database
  //-------------------------------------------------------
  // FIXME: pulled out of HTMLDriver
  if (MakeDir(args.db_dir.c_str()) != 0) {
    exit(1);
  }

  if (!args.configurationFileMode) {
    string configfnm = args.db_dir + "/config.xml";
    std::ostream* os = IOUtil::OpenOStream(configfnm.c_str());
    driver.write_config(*os);
    IOUtil::CloseStream(os);
  }

  if (args.db_copySrcFiles) {
    DIAG_Msg(1, "Copying source files reached by REPLACE/PATH statements to " << args.db_dir);
    
    // Note that this may modify file names in the ScopeTree
    copySourceFiles(scopeTree.GetRoot(), args.searchPathTpls, args.db_dir);
  }

  if (!args.outFilename_CSV.empty()) {
    const string& fnm = args.outFilename_CSV;
    DIAG_Msg(1, "Writing final scope tree (in CSV) to " << fnm);
    string fpath = args.db_dir + "/" + fnm;
    const char* osnm = (fnm == "-") ? NULL : fpath.c_str();
    std::ostream* os = IOUtil::OpenOStream(osnm);
    driver.CSV_Dump(scopeTree.GetRoot(), *os);
    IOUtil::CloseStream(os);
  } 

  if (!args.outFilename_TSV.empty()) {
    const string& fnm = args.outFilename_TSV;
    DIAG_Msg(1, "Writing final scope tree (in TSV) to " << fnm);
    string fpath = args.db_dir + "/" + fnm;
    const char* osnm = (fnm == "-") ? NULL : fpath.c_str();
    std::ostream* os = IOUtil::OpenOStream(osnm);
    driver.TSV_Dump(scopeTree.GetRoot(), *os);
    IOUtil::CloseStream(os);
  }
  
  if (args.outFilename_XML != "no") {
    int flg = (args.metrics_computeInteriorValues) ? 0 : PgmScopeTree::DUMP_LEAF_METRICS;

    const string& fnm = args.outFilename_XML;
    DIAG_Msg(1, "Writing final scope tree (in XML) to " << fnm);
    string fpath = args.db_dir + "/" + fnm;
    const char* osnm = (fnm == "-") ? NULL : fpath.c_str();
    std::ostream* os = IOUtil::OpenOStream(osnm);
    driver.XML_Dump(scopeTree.GetRoot(), flg, *os);
    IOUtil::CloseStream(os);
  }

  //-------------------------------------------------------
  // Cleanup
  //-------------------------------------------------------
  
  return 0; 
} 


//****************************************************************************
//
//****************************************************************************

#define NUM_PREFIX_LINES 2

static string 
buildConfFile(const string& hpcHome, const string& confFile);

static void 
appendContents(std::ofstream &dest, const char *srcFile);

static void
readConfFile(Args& args, Prof::MetricDescMgr& metricMgr)
{
  InitXerces(); // exits iff failure 

  const string& cfgFile = args.configurationFile;
  DIAG_Msg(2, "Initializing from: " << cfgFile);
  
  string tmpFile = buildConfFile(args.hpcHome, cfgFile);
  
  try {
    XercesErrorHandler errHndlr(cfgFile, tmpFile, NUM_PREFIX_LINES, true);
    ConfigParser parser(tmpFile, errHndlr);
    parser.parse(args, metricMgr);
  }
  catch (const SAXParseException& x) {
    unlink(tmpFile.c_str());
    //DIAG_EMsg(XMLString::transcode(x.getMessage()));
    exit(1);
  }
  catch (const ConfigParserException& x) {
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

  FiniXerces();
}


static string
buildConfFile(const string& hpcHome, const string& confFile) 
{
  string tmpFile = TmpFileName(); 
  string hpcloc = hpcHome;
  if (hpcloc[hpcloc.length()-1] != '/') {
    hpcloc += "/";
  }
  std::ofstream tmp(tmpFile.c_str(), std::ios_base::out);

  if (tmp.fail()) {
    DIAG_Throw("Unable to write temporary file: " << tmpFile);
  }
  
  // the number of lines added below must equal NUM_PREFIX_LINES
  tmp << "<?xml version=\"1.0\"?>" << std::endl 
      << "<!DOCTYPE HPCVIEW SYSTEM \"" << hpcloc // has trailing '/'
      << "share/hpctoolkit/dtd/HPCView.dtd\">" << std::endl;

  //std::cout << "TMP DTD file: '" << tmpFile << "'" << std::std::endl;
  //std::cout << "  " << hpcloc << std::endl;

  appendContents(tmp, confFile.c_str());
  tmp.close();
  return tmpFile; 
}


static void 
appendContents(std::ofstream &dest, const char *srcFile)
{
#define MAX_IO_SIZE (64 * 1024)

  std::ifstream src(srcFile);
  if (src.fail()) {
    DIAG_Throw("Unable to read file: " << srcFile);
  }

  char buf[MAX_IO_SIZE]; 
  for(; !src.eof(); ) {
    src.read(buf, MAX_IO_SIZE);

    ssize_t nRead = src.gcount();
    if (nRead == 0) break;
    dest.write(buf, nRead); 
    if (dest.fail()) {
      DIAG_Throw("appendContents: failed!");
    }
  } 
  src.close();
}


//****************************************************************************
//
//****************************************************************************

static pair<int, string>
matchFileWithPath(const string& filenm, const Analysis::PathTupleVec& pathVec);


static bool 
CSF_ScopeFilter(const ScopeInfo& x, long type)
{
  return (x.Type() == ScopeInfo::FILE || x.Type() == ScopeInfo::ALIEN);
}

// 'copySourceFiles': For every file FileScope and AlienScope in
// 'pgmScope' that can be reached with paths in 'pathVec', copy F to
// its appropriate viewname path and update F's path to be relative to
// this location.
static bool
copySourceFiles(PgmScope* pgmScope, const Analysis::PathTupleVec& pathVec,
		const string& dstDir)
{
  bool noError = true;

  // Alien scopes mean that we may attempt to copy the same file many
  // times.  Prevent multiple copies of the same file.
  std::map<string, string> copiedFiles;

  ScopeInfoFilter filter(CSF_ScopeFilter, "CSF_ScopeFilter", 0);
  for (ScopeInfoIterator it(pgmScope, &filter); it.Current(); ++it) {
    ScopeInfo* scope = it.CurScope();
    FileScope* fileScope = NULL;
    AlienScope* alienScope = NULL;

    // Note: 'fnm_orig' will be not be absolute if it is not possible to find
    // the file on the current filesystem. (cf. NodeRetriever::MoveToFile)
    string fnm_orig;
    if (scope->Type() == ScopeInfo::FILE) {
      fileScope = dynamic_cast<FileScope*>(scope);
      fnm_orig = fileScope->name();
    }
    else if (scope->Type() == ScopeInfo::ALIEN) {
      alienScope = dynamic_cast<AlienScope*>(scope);
      fnm_orig = alienScope->fileName();
    }
    else {
      DIAG_Die(DIAG_Unimplemented);
    }
    
    string fnm_new;

    // ------------------------------------------------------
    // Given fnm_orig, find fnm_new. (Use PATH information.)
    // ------------------------------------------------------
    std::map<string, string>::iterator it = copiedFiles.find(fnm_orig);
    if (it != copiedFiles.end()) {
      fnm_new = it->second;
    }
    else {
      pair<int, string> fnd = matchFileWithPath(fnm_orig, pathVec);
      int idx = fnd.first;

      if (idx >= 0) {

	string  path_fnd = pathVec[idx].first; // a real copy
	string& fnm_fnd  = fnd.second;         // just an alias
	const string& viewnm   = pathVec[idx].second;

	// canonicalize path_fnd and fnm_fnd
	if (is_recursive_path(path_fnd.c_str())) {
	  path_fnd[path_fnd.length()-RECURSIVE_PATH_SUFFIX_LN] = '\0';
	}
	path_fnd = RealPath(path_fnd.c_str());
	fnm_fnd = RealPath(fnm_fnd.c_str());

	// INVARIANT 1: fnm_fnd is an absolute path
	// INVARIANT 2: path_fnd must be a prefix of fnm_fnd
	  
	// find (fnm_fnd - path_fnd)
	char* path_sfx = const_cast<char*>(fnm_fnd.c_str());
	path_sfx = &path_sfx[path_fnd.length()];
	while (path_sfx[0] != '/') { --path_sfx; } // should start with '/'
	
	// Create new file name and copy commands
	fnm_new = "./" + viewnm + path_sfx;
	
	string fnm_to;
	if (dstDir[0]  != '/') {
	  fnm_to = "./";
	}
	fnm_to = fnm_to + dstDir + "/" + viewnm + path_sfx;
	string dir_to(fnm_to); // need to strip off ending filename to 
	uint end;              // get full path for 'fnm_to'
	for (end = dir_to.length() - 1; dir_to[end] != '/'; end--) { }
	dir_to[end] = '\0';    // should not end with '/'
	
	string cmdMkdir = "mkdir -p " + dir_to;
	string cmdCp    = "cp -f " + fnm_fnd + " " + fnm_to;
	//cerr << cmdCp << std::endl;
	
	// could use CopyFile; see StaticFiles::Copy
	if (system(cmdMkdir.c_str()) == 0 && system(cmdCp.c_str()) == 0) {
	  DIAG_Msg(1, "  " << fnm_to);
	} 
	else {
	  DIAG_EMsg("copying: '" << fnm_to);
	}
      }
    }

    // ------------------------------------------------------
    // Use find fnm_new
    // ------------------------------------------------------
    if (!fnm_new.empty()) {
      if (fileScope) {
	fileScope->SetName(fnm_new);
      }
      else {
	alienScope->fileName(fnm_new);
      }
    }
    
    copiedFiles.insert(make_pair(fnm_orig, fnm_new));
  }
  
  return noError;
}


// 'matchFileWithPath': Given a file name 'filenm' and a vector of
// paths 'pathVec', use 'pathfind_r' to determine which path in
// 'pathVec', if any, reaches 'filenm'.  Returns an index and string
// pair.  If a match is found, the index is an index in pathVec;
// otherwise it is negative.  If a match is found, the string is the
// found file name.
static pair<int, string>
matchFileWithPath(const string& filenm, const Analysis::PathTupleVec& pathVec)
{
  // Find the index to the path that reaches 'filenm'.
  // It is possible that more than one path could reach the same
  //   file because of substrings.
  //   E.g.: p1: /p1/p2/p3/*, p2: /p1/p2/*, f: /p1/p2/p3/f.c
  //   Choose the path that is most qualified (We assume RealPath length
  //   is valid test.) 
  int foundIndex = -1; // index into 'pathVec'
  int foundPathLn = 0; // length of the path represented by 'foundIndex'
  string foundFnm; 

  for (uint i = 0; i < pathVec.size(); i++) {
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
    
    const char* fnd_fnm = pathfind_r(curPath.c_str(), curFile, "r");
    if (fnd_fnm) {
      bool update = false;
      if (foundIndex < 0) {
	update = true;
      }
      else if ((foundIndex >= 0) && (realPathLn > foundPathLn)) {
	update = true;
      }
      
      if (update) {
	foundIndex = i;
	foundPathLn = realPathLn;
	foundFnm = fnd_fnm;
      }
    }
  }
  return make_pair(foundIndex, foundFnm);
}

