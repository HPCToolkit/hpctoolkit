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

#include <limits.h> /* for 'PATH_MAX' */

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/errno.h>

//*************************** User Include Files ****************************

#include "Util.hpp"

#include <lib/support/diagnostics.h>
#include <lib/support/pathfind.h>

//*************************** Forward Declarations **************************

//***************************************************************************

//***************************************************************************
// 
//***************************************************************************

Analysis::Util::ProfType_t 
Analysis::Util::getProfileType(const std::string& filenm)
{
  // FIXME: a better way of doing this would be to read the first 32
  // bytes and test for the magic cookie.
  
  // FIXME: not yet abstracted since csprof is still a mess
  static const string CALLPATH_SFX = ".csp";
  
  if (filenm.length() > CALLPATH_SFX.length()) {
    uint begpos = filenm.length() - CALLPATH_SFX.length();
    if (filenm.find(CALLPATH_SFX, begpos) != string::npos) {
      return ProfType_CALLPATH;
    }
  }

#if 0
  // version 1
  char* buf = new char[32+1];
  int bytesRead = ::fread(f, buf, sizeof(char), 32);

  // version 2
  std::ifstream in(fname);
  in.get()...

  // version 3
  std::ifstream in(fname);
  std::istreambuf_iterator<char> i(in);
  std::istreambuf_iterator<char> eos;
  std::vector<char> v(i, eos);
  // or: string str(i, i+32);
#endif
  
  return ProfType_FLAT;
}


//***************************************************************************
// 
//***************************************************************************

#include <stack>

#define DEB_NORM_SEARCH_PATH  0
#define DEB_MKDIR_SRC_DIR 0


void 
copySourceFiles(Prof::CSProfNode* node, 
		Analysis::PathTupleVec& pathVec,
		const string& dstDir);

void 
innerCopySourceFiles(Prof::CSProfNode* node, 
		     Analysis::PathTupleVec& pathVec,
		     const string& dstDir);



std::string 
normalizeFilePath(const std::string& filePath);

std::string 
normalizeFilePath(const std::string& filePath, 
		  std::stack<std::string>& pathSegmentsStack);

void 
breakPathIntoSegments(const std::string& normFilePath, 
		      std::stack<std::string>& pathSegmentsStack);



/** Copies the source files for the executable into the database 
    directory. */
void 
Analysis::Util::copySourceFiles(Prof::CallPath::Profile* prof, 
				Analysis::PathTupleVec& pathVec,
				const string& dstDir) 
{
  Prof::CCT::Tree* cct = prof->cct();
  if (!cct) { return ; }

  string dbSrcDir = dstDir + "/src";
  if (mkdir(dbSrcDir.c_str(),
	    S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) != 0) {
    DIAG_Die("could not create database source code directory " << dbSrcDir);
  }

  dbSrcDir = normalizeFilePath(dbSrcDir);
  copySourceFiles(cct->root(), pathVec, dbSrcDir);
}


/** Perform DFS traversal of the tree nodes and copy
    the source files for the executable into the database
    directory. */
void 
copySourceFiles(Prof::CSProfNode* node, 
		Analysis::PathTupleVec& pathVec,
		const string& dstDir) 
{
  xDEBUG(DEB_MKDIR_SRC_DIR, 
	 cerr << "descend into node" << std::endl;);

  if (!node) {
    return;
  }

  // For each immediate child of this node...
  for (Prof::CSProfNodeChildIterator it(node); it.CurNode(); it++) {
    // recur 
    copySourceFiles(it.CurNode(), pathVec, dstDir);
  }

  innerCopySourceFiles(node, pathVec, dstDir);
}


void 
innerCopySourceFiles(Prof::CSProfNode* node, 
		     Analysis::PathTupleVec& pathVec,
		     const string& dstDir)
{
  bool inspect; 
  string nodeSourceFile;
  string relativeSourceFile;
  string procedureFrame;
  bool sourceFileWasCopied = false;
  bool fileIsText = false; 

  switch(node->GetType()) {
  case Prof::CSProfNode::CALLSITE:
    {
      Prof::CSProfCallSiteNode* c = dynamic_cast<Prof::CSProfCallSiteNode*>(node);
      nodeSourceFile = c->GetFile();
      fileIsText = c->FileIsText();
      inspect = true;
      procedureFrame = c->GetProc();
      xDEBUG(DEB_MKDIR_SRC_DIR,
	     cerr << "will analyze CALLSITE for proc" << procedureFrame
	     << nodeSourceFile << 
	     "textFile " << fileIsText << std::endl;);
    }
    break;

  case Prof::CSProfNode::STATEMENT:
    {
      Prof::CSProfStatementNode* st = dynamic_cast<Prof::CSProfStatementNode*>(node);
      nodeSourceFile = st->GetFile();
      fileIsText = st->FileIsText();
      inspect = true;
      procedureFrame = st->GetProc();
      xDEBUG(DEB_MKDIR_SRC_DIR,
	     cerr << "will analyze STATEMENT for proc" << procedureFrame
	     << nodeSourceFile << 
	     "textFile " << fileIsText << std::endl;);
    }
    break;

  case Prof::CSProfNode::PROCEDURE_FRAME:
    {
      Prof::CSProfProcedureFrameNode* pf = 
	dynamic_cast<Prof::CSProfProcedureFrameNode*>(node);
      nodeSourceFile = pf->GetFile();
      fileIsText = pf->FileIsText();
      inspect = true;
      procedureFrame = pf->GetProc();
      xDEBUG(DEB_MKDIR_SRC_DIR,
	     cerr << "will analyze PROCEDURE_FRAME for proc" << 
	     procedureFrame << nodeSourceFile << 
	     "textFile " << fileIsText << std::endl;);
    }
    break;

  default:
    inspect = false;
    break;
  } 
  
  if (inspect) {
    // copy source file for current node
    xDEBUG(DEB_MKDIR_SRC_DIR,
	   cerr << "attempt to copy " << nodeSourceFile << std::endl;);
// FMZ     if (! nodeSourceFile.empty()) {
     if (fileIsText && !nodeSourceFile.empty()) {
      xDEBUG(DEB_MKDIR_SRC_DIR,
	     cerr << "attempt to copy text, nonnull " << nodeSourceFile << std::endl;);
      
      // foreach  search paths
      //    normalize  searchPath + file
      //    if (file can be opened) 
      //    break into components 
      //    duplicate directory structure (mkdir segment by segment)
      //    copy the file (use system syscall)
      int ii;
      bool searchDone = false;
      for (ii=0; !searchDone && ii<pathVec.size(); ii++) {
	string testPath;
	if ( nodeSourceFile[0] == '/') {
	  testPath = nodeSourceFile;
	  searchDone = true;
	}  else {
	  testPath = pathVec[ii].first +"/"+nodeSourceFile;
	}
	xDEBUG(DEB_MKDIR_SRC_DIR,
	       cerr << "search test path " << testPath << std::endl;);
	string normTestPath = normalizeFilePath(testPath);
	xDEBUG(DEB_MKDIR_SRC_DIR,
	       cerr << "normalized test path " << normTestPath << std::endl;);
	if (normTestPath == "") {
	  xDEBUG(DEB_MKDIR_SRC_DIR,
		 cerr << "null test path " << std::endl;);
	} else {
	  xDEBUG(DEB_MKDIR_SRC_DIR,
		 cerr << "attempt to text open" << normTestPath << std::endl;);
	  FILE *sourceFileHandle = fopen(normTestPath.c_str(), "rt");
	  if (sourceFileHandle != NULL) {
	    searchDone = true;
	    char normTestPathChr[PATH_MAX+1];
	    strcpy(normTestPathChr, normTestPath.c_str());
	    relativeSourceFile = normTestPathChr+1;
	    sourceFileWasCopied = true;
	    xDEBUG(DEB_MKDIR_SRC_DIR,
		   cerr << "text open succeeded; path changed to " <<
		   relativeSourceFile << std::endl;);
	    fclose (sourceFileHandle);

	    // check if the file already exists (we've copied it for a previous sample)
	    string testFilePath = dstDir + normTestPath;
	    FILE *testFileHandle = fopen(testFilePath.c_str(), "rt");
	    if (testFileHandle != NULL) {
	      fclose(testFileHandle);
	    } else {
	      // we've found the source file and we need to copy it into the database
	      std::stack<string> pathSegmentsStack;
	      breakPathIntoSegments (normTestPath,
				     pathSegmentsStack);
	      std::vector<string> pathSegmentsVector;
	      for (; !pathSegmentsStack.empty();) {
		string crtSegment = pathSegmentsStack.top();
		pathSegmentsStack.pop(); 
		pathSegmentsVector.insert(pathSegmentsVector.begin(),
					  crtSegment);
		xDEBUG(DEB_MKDIR_SRC_DIR,
		       cerr << "inserted path segment " << 
		       pathSegmentsVector[0] << std::endl;);
	      }

	      xDEBUG(DEB_MKDIR_SRC_DIR,
		     cerr << "converted stack to vector" << std::endl;);

	      char filePathChr[PATH_MAX +1];
	      strcpy(filePathChr, dstDir.c_str());
	      chdir(filePathChr);

	      xDEBUG(DEB_MKDIR_SRC_DIR,
		     cerr << "after chdir " << std::endl;);

	      string subPath = dstDir;
	      int pathSegmentIndex;
	      for (pathSegmentIndex=0; 
		   pathSegmentIndex<pathSegmentsVector.size()-1;
		   pathSegmentIndex++) {
		subPath = subPath+"/"+pathSegmentsVector[pathSegmentIndex];
		xDEBUG(DEB_MKDIR_SRC_DIR,
		       cerr << "about to mkdir " 
		       << subPath << std::endl;);
		int mkdirResult =  
		  mkdir (subPath.c_str(), 
			 S_IRWXU | S_IRGRP | S_IXGRP 
			 | S_IROTH | S_IXOTH); 
		if (mkdirResult == -1) {
		  switch (errno) {
		  case EEXIST:  
		    xDEBUG(DEB_MKDIR_SRC_DIR,
			   cerr << "EEXIST " << std::endl;); 
		    // we created the same directory 
		    // for a different source file
		    break;
		  default:
		    cerr << "mkdir failed for " << subPath << std::endl;
		    perror("mkdir failed:");
		    exit (1);
		  }
		}
	      }
	      strcpy(filePathChr, subPath.c_str());
	      chdir(filePathChr);
	      string cpCommand = "cp -f "+normTestPath+" .";
	      system (cpCommand.c_str());
	      // fix the file name  so it points to the one in the source directory
	    }
	  }
	}
      }
    }
  }
  
  if (inspect && sourceFileWasCopied) {
    switch( node->GetType()) {
    case Prof::CSProfNode::CALLSITE:
      {
	Prof::CSProfCallSiteNode* c = dynamic_cast<Prof::CSProfCallSiteNode*>(node);
	c->SetFile(relativeSourceFile);
      }
      break;
    case Prof::CSProfNode::STATEMENT:
      {
	Prof::CSProfStatementNode* st = dynamic_cast<Prof::CSProfStatementNode*>(node);
	st->SetFile(relativeSourceFile);
      }
      break;
    case Prof::CSProfNode::PROCEDURE_FRAME:
      {
	Prof::CSProfProcedureFrameNode* pf = 
	  dynamic_cast<Prof::CSProfProcedureFrameNode*>(node);
	pf->SetFile(relativeSourceFile);
      }
      break;
    default:
      cerr << "Invalid node type" << std::endl;
      exit(0);
      break;
    }
  }
}


//***************************************************************************

/** Normalizes a file path. Handle construncts such as :
 ~/...
 ./...
 .../dir1/../...
 .../dir1/./.... 
 ............./
*/
string 
normalizeFilePath(const string& filePath, 
		  std::stack<string>& pathSegmentsStack)
{
  char cwdName[PATH_MAX +1];
  getcwd(cwdName, PATH_MAX);
  string crtDir=cwdName; 
  
  string filePathString = filePath;
  char filePathChr[PATH_MAX +1];
  strcpy(filePathChr, filePathString.c_str());

  // ~/...
  if ( filePathChr[0] == '~' ) {
    if ( filePathChr[1]=='\0' ||
	 filePathChr[1]=='/' ) {
      // reference to our home directory
      string homeDir = getenv("HOME"); 
      xDEBUG(DEB_NORM_SEARCH_PATH, 
	     cerr << "home dir:" << homeDir << std::endl;);
      filePathString = homeDir + (filePathChr+1); 
      xDEBUG(DEB_NORM_SEARCH_PATH, 
	     cerr << "new filePathString=" << filePathString << std::endl;);
    } else {
      // parse the beginning of the filePath to determine the other username
      int filePos = 0;
      char userHome[PATH_MAX+1]; 
      for ( ;
	    ( filePathChr[filePos] != '\0' &&
	      filePathChr[filePos] != '/');
	    filePos++) {
	userHome[filePos]=filePathChr[filePos];
      }
      userHome[filePos]='\0';
      if (chdir (userHome) == 0 ) {
	char userHomeDir[PATH_MAX +1];
	getcwd(userHomeDir, PATH_MAX);
	filePathString = userHomeDir;
	filePathString = filePathString + (filePathChr + filePos);
	xDEBUG(DEB_NORM_SEARCH_PATH, 
	       cerr << "new filePathString=" << 
	       filePathString << std::endl;);
      }
    }
  }

 
  // ./...
  if ( filePathChr[0] == '.') {
    xDEBUG(DEB_NORM_SEARCH_PATH, 
	   cerr << "crt dir:" << crtDir << std::endl;);
    filePathString = crtDir + "/"+string(filePathChr+1); 
    xDEBUG(DEB_NORM_SEARCH_PATH, 
	   cerr << "updated filePathString=" << filePathString << std::endl;);
  }

  if ( filePathChr[0] != '/') {
    xDEBUG(DEB_NORM_SEARCH_PATH, 
	   cerr << "crt dir:" << crtDir << std::endl;);
    filePathString = crtDir + "/"+ string(filePathChr); 
    xDEBUG(DEB_NORM_SEARCH_PATH, 
	   cerr << "applied crtDir: " << crtDir << " and got updated filePathString=" << filePathString << std::endl;);
  }

  // ............./
  if ( filePathChr[ strlen(filePathChr) -1] == '/') {
    strcpy(filePathChr, filePathString.c_str());
    filePathChr [strlen(filePathChr)-1]='\0';
    filePathString = filePathChr; 
    xDEBUG(DEB_NORM_SEARCH_PATH, 
	   cerr << "after removing trailing / got filePathString=" << filePathString << std::endl;);
  }

  // parse the path and determine segments separated by '/' 
  strcpy(filePathChr, filePathString.c_str());

  int crtPos=0;
  char crtSegment[PATH_MAX+1];
  int crtSegmentPos=0;
  crtSegment[crtSegmentPos]='\0';

  for (; crtPos <= strlen(filePathChr); crtPos ++) {
    xDEBUG(DEB_NORM_SEARCH_PATH, 
	   cerr << "parsing " << filePathChr[crtPos] << std::endl;);
    if (filePathChr[crtPos] ==  '/' || 
	filePathChr[crtPos] ==  '\0') {
      crtSegment[crtSegmentPos]='\0';
      xDEBUG(DEB_NORM_SEARCH_PATH, 
	     cerr << "parsed segment " << crtSegment << std::endl;);
      if (strcmp(crtSegment,".") == 0) {
	//  .../dir1/./.... 
      } 
      else if (strcmp(crtSegment,"..") == 0) {
	// .../dir1/../...
	if (pathSegmentsStack.empty()) {
	  DIAG_EMsg("Invalid path: " << filePath);
	  return "";
	} 
	else {
	  pathSegmentsStack.pop();
	}
      } 
      else {
	if (crtSegmentPos>0) {
	  pathSegmentsStack.push(string(crtSegment));
	  xDEBUG(DEB_NORM_SEARCH_PATH, 
		 cerr << "pushed segment " << crtSegment << std::endl;);
	}
      }
      crtSegment[0]='\0';
      crtSegmentPos=0;
    } 
    else {
      crtSegment[crtSegmentPos] = filePathChr[crtPos];
      crtSegmentPos++;
    }
  }

  if (pathSegmentsStack.empty()) {
    DIAG_EMsg("Invalid path: " << filePath);
    return "";
  }

  std::stack<string> stackCopy = pathSegmentsStack;
  string resultPath = stackCopy.top();
  stackCopy.pop();
  for (;! stackCopy.empty(); ) {
    resultPath = stackCopy.top() + "/"+resultPath;
    stackCopy.pop();
  }
  resultPath = "/"+resultPath;
  xDEBUG(DEB_NORM_SEARCH_PATH, 
	 cerr << "normalized path: " << resultPath << std::endl;);
  return resultPath;
}


string 
normalizeFilePath(const string& filePath)
{
  std::stack<string> pathSegmentsStack;
  string resultPath = normalizeFilePath(filePath, pathSegmentsStack);
  return resultPath;
}


/** Decompose a normalized path into path segments.*/
void 
breakPathIntoSegments(const string& normFilePath, 
			   std::stack<string>& pathSegmentsStack)
{
  string resultPath = normalizeFilePath(normFilePath, pathSegmentsStack);
}


//***************************************************************************
// 
//***************************************************************************


static std::pair<int, string>
matchFileWithPath(const string& filenm, const Analysis::PathTupleVec& pathVec);

static string
copySourceFile(const string& filenm, const string& dstDir, 
	       const Analysis::PathTuple& pathTpl);


static bool 
CSF_ScopeFilter(const Prof::Struct::ANode& x, long type)
{
  return (x.Type() == Prof::Struct::ANode::TyFILE || x.Type() == Prof::Struct::ANode::TyALIEN);
}

// copySourceFiles: For every Prof::Struct::File and
// Prof::Struct::Alien x in 'pgmScope' that can be reached with paths in
// 'pathVec', copy x to its appropriate viewname path and update x's
// path to be relative to this location.
void
Analysis::Util::copySourceFiles(Prof::Struct::Pgm* structure, 
				const Analysis::PathTupleVec& pathVec,
				const string& dstDir)
{
  // Alien scopes mean that we may attempt to copy the same file many
  // times.  Prevent multiple copies of the same file.
  std::map<string, string> copiedFiles;

  Prof::Struct::ANodeFilter filter(CSF_ScopeFilter, "CSF_ScopeFilter", 0);
  for (Prof::Struct::ANodeIterator it(structure, &filter); it.Current(); ++it) {
    Prof::Struct::ANode* strct = it.CurScope();
    Prof::Struct::File* fileStrct = NULL;
    Prof::Struct::Alien* alienStrct = NULL;

    // Note: 'fnm_orig' will be not be absolute if it is not possible to find
    // the file on the current filesystem. (cf. TreeInterface::MoveToFile)
    string fnm_orig;
    if (strct->Type() == Prof::Struct::ANode::TyFILE) {
      fileStrct = dynamic_cast<Prof::Struct::File*>(strct);
      fnm_orig = fileStrct->name();
    }
    else if (strct->Type() == Prof::Struct::ANode::TyALIEN) {
      alienStrct = dynamic_cast<Prof::Struct::Alien*>(strct);
      fnm_orig = alienStrct->fileName();
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
      std::pair<int, string> fnd = matchFileWithPath(fnm_orig, pathVec);
      int idx = fnd.first;

      if (idx >= 0) {
	fnm_new = copySourceFile(fnd.second, dstDir, pathVec[idx]);
      }
    }

    // ------------------------------------------------------
    // Use find fnm_new
    // ------------------------------------------------------
    if (!fnm_new.empty()) {
      if (fileStrct) {
	fileStrct->SetName(fnm_new);
      }
      else {
	alienStrct->fileName(fnm_new);
      }
    }
    
    copiedFiles.insert(make_pair(fnm_orig, fnm_new));
  }
}


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
	foundFnm = fnd_fnm;
      }
    }
  }
  return make_pair(foundIndex, foundFnm);
}


// Given a file 'filenm' a destination directory 'dstDir' and a
// PathTuple, form a database file name, copy 'filenm' into the
// database and return the database file name.
static string
copySourceFile(const string& filenm, const string& dstDir, 
	       const Analysis::PathTuple& pathTpl)
{
#define SEARCHPATH_IS_SUFFIX_OF_FILE 0

  string fnm_fnd = RealPath(filenm.c_str());
  const string& viewnm = pathTpl.second;

#if (SEARCHPATH_IS_SUFFIX_OF_FILE) 
  // canonicalize path_fnd
  string path_fnd = pathTpl.first; // a real copy
  if (is_recursive_path(path_fnd.c_str())) {
    path_fnd[path_fnd.length()-RECURSIVE_PATH_SUFFIX_LN] = '\0';
  }
  path_fnd = RealPath(path_fnd.c_str());

  // INVARIANT 1: fnm_fnd is an absolute path
  // INVARIANT 2: path_fnd must be a prefix of fnm_fnd

  // tallent: actually #2 may not be true with symbolic links and '..':
  //   path_fnd: /.../codes/NAMD_2.6_Source/charm-5.9/mpi-linux-amd64
  //   filenm:   /.../codes/NAMD_2.6_Source/charm-5.9/mpi-linux-amd64/../bin/../include/LBComm.h
  //   fnm_fnd:  /.../codes/NAMD_2.6_Source/charm-5.9/src/ck-ldb/LBComm.h


  // find (fnm_fnd - path_fnd)
  const char* path_sfx = fnm_fnd.c_str();
  path_sfx = &path_sfx[path_fnd.length()];
  while (path_sfx[0] != '/') { --path_sfx; } // should start with '/'
#else
  // INVARIANT: fnm_fnd.length() > 1
  const char* path_sfx = fnm_fnd.c_str();
#endif
	
  // Create new file name and copy commands
  string fnm_new = "./" + viewnm + path_sfx;
	
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

  return fnm_new;
}


//****************************************************************************
