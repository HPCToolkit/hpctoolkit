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
// Copyright ((c)) 2002-2021, Rice University
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
//   $HeadURL$
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

#include <string>
using std::string;

#include <algorithm>
#include <typeinfo>

#include <cstring> // strlen()

#include <dirent.h> // scandir()

//*************************** User Include Files ****************************

#include <include/gcc-attr.h>
#include <include/uint.h>

#include "Util.hpp"

#include <lib/banal/StructSimple.hpp>

#include <lib/prof/pms-format.h>
#include <lib/prof/cms-format.h>

#include <lib/prof-lean/hpcio.h>
#include <lib/prof-lean/hpcfmt.h>
#include <lib/prof-lean/hpcrun-fmt.h>
#include <lib/prof-lean/hpcrunflat-fmt.h>
#include <lib/prof-lean/tracedb.h>

#include <lib/support/PathFindMgr.hpp>
#include <lib/support/PathReplacementMgr.hpp>
#include <lib/support/diagnostics.h>
#include <lib/support/dictionary.h>
#include <lib/support/realpath.h>

#define DEBUG_DEMAND_STRUCT  0
#define TMP_BUFFER_LEN 1024

//***************************************************************************

static inline int
fileExtensionFilter(const struct dirent* entry,
		    const string& ext, uint extLen)
{
  // FileUtil::fnmatch("*.hpcrun", entry->d_name);
  uint nmLen = strlen(entry->d_name);
  if (nmLen > extLen) {
    int cmpbeg = (nmLen - extLen);
    return (strncmp(&entry->d_name[cmpbeg], ext.c_str(), extLen) == 0);
  }
  return 0;
}


static int 
hpcrunFileFilter(const struct dirent* entry)
{
  static const string ext = string(".") + HPCRUN_ProfileFnmSfx;
  static const uint extLen = ext.length();

  return fileExtensionFilter(entry, ext, extLen);
}


#if 0
static int 
hpctraceFileFilter(const struct dirent* entry)
{
  static const string ext = string(".") + HPCRUN_TraceFnmSfx;
  static const uint extLen = ext.length();

  return fileExtensionFilter(entry, ext, extLen);
}
#endif


//***************************************************************************
// 
//***************************************************************************

namespace Analysis {
namespace Util {

Analysis::Util::ProfType_t 
getProfileType(const std::string& filenm)
{
  static const int bufSZ = 32;
  char buf[bufSZ] = { '\0' };

  std::istream* is = IOUtil::OpenIStream(filenm.c_str());
  is->read(buf, bufSZ);
  IOUtil::CloseStream(is);
  
  ProfType_t ty = ProfType_NULL;
  if (strncmp(buf, HPCRUN_FMT_Magic, HPCRUN_FMT_MagicLen) == 0) {
    ty = ProfType_Callpath;
  }
  else if (strncmp(buf, HPCMETRICDB_FMT_Magic, HPCMETRICDB_FMT_MagicLen) == 0) {
    ty = ProfType_CallpathMetricDB;
  }
  else if (strncmp(buf, HPCTRACE_FMT_Magic, HPCTRACE_FMT_MagicLen) == 0) {
    ty = ProfType_CallpathTrace;
  }
  else if (strncmp(buf, HPCRUNFLAT_FMT_Magic, HPCRUNFLAT_FMT_MagicLen) == 0) {
    ty = ProfType_Flat;
  }else if(filenm.find(".sparse-db") != std::string::npos){ //YUMENG: is->read didn't work, may need to FIX later
    ty = ProfType_SparseDBtmp;
  }else if(strncmp(buf, HPCPROFILESPARSE_FMT_Magic, HPCPROFILESPARSE_FMT_MagicLen) == 0){ 
    ty = ProfType_SparseDBthread;
  }else if(strncmp(buf, HPCCCTSPARSE_FMT_Magic, HPCCCTSPARSE_FMT_MagicLen) == 0){ 
    ty = ProfType_SparseDBcct;
  }else if(strncmp(buf, HPCTRACEDB_FMT_Magic, HPCTRACEDB_FMT_MagicLen) == 0){ 
    ty = ProfType_TraceDB;
  }

  
  return ty;
}

} // end of Util namespace
} // end of Analysis namespace


//***************************************************************************
// 
//***************************************************************************

namespace Analysis {
namespace Util {

NormalizeProfileArgs_t
normalizeProfileArgs(const StringVec& inPaths)
{
  NormalizeProfileArgs_t out;

  for (StringVec::const_iterator it = inPaths.begin(); 
      it != inPaths.end(); ++it) {
    std::string path = *it; // copy
    if (FileUtil::isDir(path.c_str())) {
      // ensure 'path' ends in '/'
      if (path[path.length() - 1] != '/') {
        path += "/";
      }

      struct dirent** dirEntries = NULL;
      int dirEntriesSz = scandir(path.c_str(), &dirEntries,
          hpcrunFileFilter, alphasort);
      if (dirEntriesSz < 0) {
        DIAG_Throw("could not read directory: " << path);
      }
      else {
        out.groupMax++; // obtain next group;
        for (int i = 0; i < dirEntriesSz; ++i) {
          string nm = path + dirEntries[i]->d_name;
          free(dirEntries[i]);
          out.paths->push_back(nm);
          out.pathLenMax = std::max(out.pathLenMax, (uint)nm.length());
          out.groupMap->push_back(out.groupMax);
        }
        free(dirEntries);
      }
      // TODO: collect group
    }
    else {
      out.groupMax++; // obtain next group;
      out.paths->push_back(path);
      out.pathLenMax = std::max(out.pathLenMax, (uint)path.length());
      out.groupMap->push_back(out.groupMax);
    }
  }

  return out;
}

} // end of Util namespace
} // end of Analysis namespace


//***************************************************************************
// 
//***************************************************************************

namespace Analysis {
namespace Util {

// Parses a string to get 2 values, 'oldVal' and 'newVal', which are 
// syntatically separated by a '=' character. To avoid confusions with '=' 
// characters in the path name, convention is for the user to escape all '=',
// and only '=' characters,in their path.
//
// @param arg: A string with only '=' characters escaped
// @return: How many occurances of unescaped '=' characters there are.
//          A value other than 1 indicates an error
int
parseReplacePath(const std::string& arg)
{
  size_t in = arg.find_first_of('=');
  size_t indexOfEqual = 0;
  int numEquals = 0;
  int numBackslashesBeforeTrueEqual = 0;
  
  size_t trailingIn = 0;
  string trueArg; // arg with the '\' stripped out
  while (in != arg.npos) {
    if (arg[in-1] != '\\') { // indicates the true equals character
      numEquals++;
      trueArg += arg.substr(trailingIn, in - trailingIn + 1);
      indexOfEqual = in;
    }
    else {
      if (indexOfEqual == 0) {
	numBackslashesBeforeTrueEqual++;
      }
      
      // to copy everything up till the '\', and then add on a '='
      // that is known
      trueArg += arg.substr(trailingIn, in - trailingIn - 1);
      trueArg += '=';
    }
    
    trailingIn = in+1;
    in = arg.find_first_of('=', (in + 1));
  }
  
  
  if (numEquals == 1) {
    trueArg += arg.substr(arg.find_last_of('=') + 1);
    indexOfEqual -= numBackslashesBeforeTrueEqual; // b/c '/' were removed
    
    string oldVal = trueArg.substr(0, indexOfEqual);
    string newVal = trueArg.substr(indexOfEqual + 1);
    
    PathReplacementMgr::singleton().addPath(oldVal, newVal);
  }
  
  return numEquals;
}


} // end of Util namespace
} // end of Analysis namespace


//***************************************************************************
// 
//***************************************************************************

namespace Analysis {
namespace Util {

// Always consult the load module's structure information (Struct::LM)
// and perform a lookup by VMA first.  If this fails:
//
//   - If the structure was seeded by full structure by
//     Struct::makeStructure (useStruct), then use the "unknown" file
//     procedures and
//
//   - Otherwise, create structure using BAnal::Struct::makeStructureSimple.
//
// In either case, return a Struct::Stmt.  This is important because
// it ensures that every sample maps to a Stmt, a fact that is
// exploited by Analysis::CallPath::noteStaticStructureOnLeaves().
//
// The above policy assumes that when full structure has been
// provided, BAnal::Struct::makeStructureSimple could have undesirable
// effects.  One example of a problem is that for simple structure, a
// line -> Struct::Stmt map is consulted which is ambiguous in the
// presence of inline (Struct::Alien).
//
// Note: now with precomputeStructSimple(), except for hpcprof-flat,
// binutils 'lm' is always NULL and we only ever use Prof lmStruct.
// With precompute, findStmt() should always find the stmt, but if not
// (eg, gap in full struct file), then the answer has to be unknown.
//
Prof::Struct::ACodeNode*
demandStructure(VMA vma, Prof::Struct::LM* lmStruct,
		BinUtil::LM* lm, bool useStruct,
		const string* unknownProcNm)
{
  using namespace Prof;
  using namespace std;

  Struct::ACodeNode * stmt = lmStruct->findStmt(vma);
  bool found = (stmt != NULL);

  if (! found) {
    if (lm != NULL) {
      // only for hpcprof-flat
      stmt = BAnal::Struct::makeStructureSimple(lmStruct, lm, vma);
    }
    else {
      char tmp_buf[TMP_BUFFER_LEN];
      string unknown_proc = (unknownProcNm) ? *unknownProcNm : string(UNKNOWN_PROC);
      if (unknown_proc == UNKNOWN_PROC) {
        snprintf(tmp_buf, TMP_BUFFER_LEN, " 0x%lx", vma);
        unknown_proc += tmp_buf;
        if (lmStruct != NULL) {
          std::string base = FileUtil::basename(lmStruct->name().c_str());
          snprintf(tmp_buf, TMP_BUFFER_LEN, " [%s]", base.c_str());
          unknown_proc += tmp_buf;
        }
      }
      Struct::File * fileStruct = Struct::File::demand(lmStruct, UNKNOWN_FILE);
      Struct::Proc * procStruct = Struct::Proc::demand(fileStruct, unknown_proc);

      stmt = procStruct->demandStmtSimple(0, vma, vma + 1);
    }
  }

#if DEBUG_DEMAND_STRUCT
  cout << "------------------------------------------------------------\n"
       << "0x" << hex << vma << dec << "  (demand struct)"
       << "  found: " << found << "\n\n";

  stmt->dumpmePath(cout, 0, "");
  cout << "\n";
#endif

  return stmt;
}

} // end of Util namespace
} // end of Analysis namespace


//***************************************************************************
// 
//***************************************************************************

static string
copySourceFileMain(const string& fnm_orig,
		   std::map<string, string>& processedFiles,
		   const Analysis::PathTupleVec& pathVec,
		   const string& dstDir);

static bool 
Flat_Filter(const Prof::Struct::ANode& x, long GCC_ATTR_UNUSED type)
{
  return (x.type() == Prof::Struct::ANode::TyFile || 
	  x.type() == Prof::Struct::ANode::TyLoop || 
	  x.type() == Prof::Struct::ANode::TyAlien);
}


namespace Analysis {
namespace Util {

// copySourceFiles: For every Prof::Struct::File and
// Prof::Struct::Alien x in 'structure' that can be reached with paths
// in 'pathVec', copy x to its appropriate viewname path and update
// x's path to be relative to this location.
void
copySourceFiles(Prof::Struct::Root* structure, 
		const Analysis::PathTupleVec& pathVec,
		const string& dstDir)
{
  // Prevent multiple copies of the same file (Alien scopes)
  std::map<string, string> processedFiles;

  Prof::Struct::ANodeFilter filter(Flat_Filter, "Flat_Filter", 0);
  for (Prof::Struct::ANodeIterator it(structure, &filter); it.Current(); ++it) {
    Prof::Struct::ANode* strct = it.current();

    // Note: 'fnm_orig' will be not be absolute if it is not possible
    // to resolve it on the current filesystem. (cf. RealPathMgr)

    // DIAG_Assert( , DIAG_UnexpectedInput);
    const string& fnm_orig = 
      ((typeid(*strct) == typeid(Prof::Struct::Alien)) ?
       dynamic_cast<Prof::Struct::Alien*>(strct)->fileName() : 
       ((typeid(*strct) == typeid(Prof::Struct::Loop)) ? 
	dynamic_cast<Prof::Struct::Loop*>(strct)->fileName() : 
	strct->name()));
    
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
      } else if (typeid(*strct) == typeid(Prof::Struct::Loop)) {
	dynamic_cast<Prof::Struct::Loop*>(strct)->fileName(fnm_new);
      } else {
	dynamic_cast<Prof::Struct::File*>(strct)->name(fnm_new);
      }
    }
  }
}

} // end of Util namespace
} // end of Analysis namespace



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
    if (PathFindMgr::isRecursivePath(curPath.c_str())) {
      realPath[realPath.length() - PathFindMgr::RecursivePathSfxLn] = '\0';
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
    
    const char* fnd_fnm = PathFindMgr::singleton().pathfind(curPath.c_str(),
							    curFile, "r");

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
	
  try {
    FileUtil::mkdir(dir_to);
    FileUtil::copy(fnm_to, fnm_fnd);
    DIAG_DevMsgIf(0, "cp " << fnm_to);
  }
  catch (const Diagnostics::Exception& x) {
    DIAG_EMsg(x.message());
  }
  
  return fnm_new;
}


//***************************************************************************
//
//***************************************************************************

namespace Analysis {
namespace Util {

// copyTraceFiles:
void
copyTraceFiles(const std::string& dstDir, const std::set<string>& srcFiles)
{
  bool tryMove = true;

  for (std::set<string>::iterator it = srcFiles.begin();
       it != srcFiles.end(); ++it) {

    const string& x = *it;

    const string  srcFnm1 = x + "." + HPCPROF_TmpFnmSfx;
    const string& srcFnm2 = x;
    const string  dstFnm = dstDir + "/" + FileUtil::basename(x);

    // Note: the source and destination directories may be on
    // different mount points.  For the trace.tmp files, we try move
    // first (faster), if that fails, try copy and delete.  If any
    // move fails, then always copy (so only one failed move).

    if (FileUtil::isReadable(srcFnm1)) {
      // trace.tmp exists: try move, then copy and delete
      bool copyDone = false;
      if (tryMove) {
	try {
	  DIAG_Msg(2, "trace (mv): '" << srcFnm1 << "' -> '" << dstFnm << "'");
	  FileUtil::move(dstFnm, srcFnm1);
	  copyDone = true;
	}
	catch (const Diagnostics::Exception& ex) {
	  DIAG_Msg(2, "trace mv failed, trying cp");
	  tryMove = false;
	}
      }
      if (! copyDone) {
	try {
	  DIAG_Msg(2, "trace (cp): '" << srcFnm1 << "' -> '" << dstFnm << "'");
	  FileUtil::copy(dstFnm, srcFnm1);
	  FileUtil::remove(srcFnm1.c_str());
	}
	catch (const Diagnostics::Exception& ex) {
	  DIAG_EMsg("While copying trace files ['"
		    << srcFnm1 << "' -> '" << dstFnm << "']:" << ex.message());
	}
      }
    }
    else {
      // no trace.tmp file: always copy (keep original)
      try {
	DIAG_Msg(2, "trace (cp): '" << srcFnm2 << "' -> '" << dstFnm << "'");
	FileUtil::copy(dstFnm, srcFnm2);
      }
      catch (const Diagnostics::Exception& ex) {
	DIAG_EMsg("While copying trace files ['"
		  << srcFnm2 << "' -> '" << dstFnm << "']:" << ex.message());
      }
    }
  }
}


} // end of Util namespace
} // end of Analysis namespace


//****************************************************************************
