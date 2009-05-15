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

//***************************************************************************
//
// File:
//   $Source$
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
using std::cerr;
using std::hex;
using std::dec;

#include <string>
using std::string;

#include <cstring>

#include <typeinfo>

//*************************** User Include Files ****************************

#include "Util.hpp"

#include <lib/banal/bloop-simple.hpp>

#include <lib/prof-lean/hpcrun-fmt.h>
#include <lib/prof-lean/hpcrunflat-fmt.h>

#include <lib/support/diagnostics.h>
#include <lib/support/pathfind.h>
#include <lib/support/realpath.h>

//*************************** Forward Declarations **************************

//***************************************************************************

//***************************************************************************
// 
//***************************************************************************

Analysis::Util::ProfType_t 
Analysis::Util::getProfileType(const std::string& filenm)
{
  static const int bufSZ = 32;
  char buf[bufSZ] = { '\0' };

  std::istream* is = IOUtil::OpenIStream(filenm.c_str());
  is->read(buf, bufSZ);
  IOUtil::CloseStream(is);
  
  ProfType_t ty = ProfType_NULL;
  if (strncmp(buf, HPCFILE_CSPROF_MAGIC_STR, 
	      HPCFILE_CSPROF_MAGIC_STR_LEN) == 0) {
    ty = ProfType_CALLPATH;      
  }
  else if (strncmp(buf, HPCRUNFILE_MAGIC_STR, 
		   HPCRUNFILE_MAGIC_STR_LEN) == 0) {
    ty = ProfType_FLAT;
  }

  return ty;
}


//***************************************************************************
// 
//***************************************************************************


// Always consult the load module's structure information
// (Struct::LM) and perform a lookup by VMA first.  If this fails:
//
//   - If the structure was seeded by full structure by
//     bloop::makeStructure (useStruct), then use the "unknown" file
//     and procedures.
//
//   - Otherwise, create structure using banal::bloop::makeStructureSimple
//
// This policy assumes that when full structure has been provided,
// banal::bloop::makeStructureSimple could have undesirable effects.  One
// example of a problem is that for simple structure, a line ->
// Struct::Stmt map is consulted which is ambiguous in the presence of
// inlinine (Struct::Alien).
Prof::Struct::ACodeNode*
Analysis::Util::demandStructure(VMA vma, Prof::Struct::LM* lmStrct, 
				BinUtil::LM* lm, bool useStruct)
{
  using namespace Prof;
  Struct::ACodeNode* strct = lmStrct->findByVMA(vma);
  if (!strct) {
    if (useStruct) {
      Struct::File* fileStrct = 
	Struct::File::demand(lmStrct, Struct::Tree::UnknownFileNm);
      strct = Struct::Proc::demand(fileStrct, Struct::Tree::UnknownProcNm);
    }
    else {
      strct = banal::bloop::makeStructureSimple(lmStrct, lm, vma);
    }
  }
  return strct;
}


//***************************************************************************
// 
//***************************************************************************

static string
copySourceFileMain(const string& fnm_orig,
		   std::map<string, string>& processedFiles,
		   const Analysis::PathTupleVec& pathVec,
		   const string& dstDir);

static bool 
Flat_Filter(const Prof::Struct::ANode& x, long type)
{
  return (x.type() == Prof::Struct::ANode::TyFILE 
	  || x.type() == Prof::Struct::ANode::TyALIEN);
}


// copySourceFiles: For every Prof::Struct::File and
// Prof::Struct::Alien x in 'structure' that can be reached with paths
// in 'pathVec', copy x to its appropriate viewname path and update
// x's path to be relative to this location.
void
Analysis::Util::copySourceFiles(Prof::Struct::Root* structure, 
				const Analysis::PathTupleVec& pathVec,
				const string& dstDir)
{
  // Prevent multiple copies of the same file (Alien scopes)
  std::map<string, string> processedFiles;

  Prof::Struct::ANodeFilter filter(Flat_Filter, "Flat_Filter", 0);
  for (Prof::Struct::ANodeIterator it(structure, &filter); it.Current(); ++it) {
    Prof::Struct::ANode* strct = it.CurNode();

    // Note: 'fnm_orig' will be not be absolute if it is not possible
    // to resolve it on the current filesystem. (cf. RealPathMgr)

    // DIAG_Assert( , DIAG_UnexpectedInput);
    const string& fnm_orig = (typeid(*strct) == typeid(Prof::Struct::Alien)) ?
      dynamic_cast<Prof::Struct::Alien*>(strct)->fileName() : strct->name();
    
    // ------------------------------------------------------
    // Given fnm_orig, attempt to find and copy fnm_new
    // ------------------------------------------------------
    string fnm_new =
      copySourceFileMain(fnm_orig, processedFiles, pathVec, dstDir);
    
    // ------------------------------------------------------
    // Update static structure
    // ------------------------------------------------------
    if (!fnm_new.empty()) {
      if (typeid(*strct) == typeid(Prof::Struct::Alien)) {
	dynamic_cast<Prof::Struct::Alien*>(strct)->fileName(fnm_new);
      }
      else {
	dynamic_cast<Prof::Struct::File*>(strct)->name(fnm_new);
      }
    }
  }
}



static std::pair<int, string>
matchFileWithPath(const string& filenm, const Analysis::PathTupleVec& pathVec);

static string
copySourceFile(const string& filenm, const string& dstDir, 
	       const Analysis::PathTuple& pathTpl);

static string
copySourceFileMain(const string& fnm_orig,
		   std::map<string, string>& processedFiles,
		   const Analysis::PathTupleVec& pathVec,
		   const string& dstDir)
{
  string fnm_new;
  
  std::map<string, string>::iterator it = processedFiles.find(fnm_orig);
  if (it != processedFiles.end()) {
    fnm_new = it->second;
  }
  else {
    std::pair<int, string> fnd = matchFileWithPath(fnm_orig, pathVec);
    int idx = fnd.first;
    if (idx >= 0) {
      // fnm_orig explicitly matches a <search-path, path-view> tuple
      fnm_new = copySourceFile(fnd.second, dstDir, pathVec[idx]);
    }
    else if (fnm_orig[0] == '/' && FileUtil::isReadable(fnm_orig.c_str())) {
      // fnm_orig does not match a pathVec tuple; but if it is an
      // absolute path that is readable, use the default <search-path,
      // path-view> tuple.
      static const Analysis::PathTuple 
	defaultTpl("/", Analysis::DefaultPathTupleTarget);
      fnm_new = copySourceFile(fnm_orig, dstDir, defaultTpl);
    }

    if (fnm_new.empty()) {
      DIAG_WMsg(2, "lost: " << fnm_orig);
    }
    else {
      DIAG_Msg(2, "  cp:" << fnm_orig << " -> " << fnm_new);
    }
    processedFiles.insert(make_pair(fnm_orig, fnm_new));
  }
  
  return fnm_new;
}


//***************************************************************************

// matchFileWithPath: Given a file name 'filenm' and a vector of paths
// 'pathVec', use 'pathfind_r' to determine which path in 'pathVec',
// if any, reaches 'filenm'.  Returns an index and string pair.  If a
// match is found, the index is an index in pathVec; otherwise it is
// negative.  If a match is found, the string is the found file name.
static std::pair<int, string>
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
	foundFnm = RealPath(fnd_fnm);
      }
    }
  }
  return make_pair(foundIndex, foundFnm);
}


// Given a file 'filenm' a destination directory 'dstDir' and a
// PathTuple, form a database file name, copy 'filenm' into the
// database and return the database file name.
// NOTE: assume filenm is already a 'real path'
static string
copySourceFile(const string& filenm, const string& dstDir, 
	       const Analysis::PathTuple& pathTpl)
{
  const string& fnm_fnd = filenm;
  const string& viewnm = pathTpl.second;

  // Create new file name and copy commands
  string fnm_new = "./" + viewnm + fnm_fnd;
	
  string fnm_to;
  if (dstDir[0]  != '/') {
    fnm_to = "./";
  }
  fnm_to = fnm_to + dstDir + "/" + viewnm + fnm_fnd;
  string dir_to(fnm_to); // need to strip off ending filename to 
  uint end;              // get full path for 'fnm_to'
  for (end = dir_to.length() - 1; dir_to[end] != '/'; end--) { }
  dir_to[end] = '\0';    // should not end with '/'
	
  string cmdMkdir = "mkdir -p " + dir_to;
  string cmdCp    = "cp -f " + fnm_fnd + " " + fnm_to;
  //cerr << cmdCp << std::endl;
  // mkdir(x, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)

	
  // could use CopyFile; see StaticFiles::Copy
  if (system(cmdMkdir.c_str()) == 0 && system(cmdCp.c_str()) == 0) {
    DIAG_DevMsgIf(0, "cp " << fnm_to);
  } 
  else {
    DIAG_EMsg("copying: '" << fnm_to);
  }

  return fnm_new;
}


//****************************************************************************
