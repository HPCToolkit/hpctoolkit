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
// Copyright ((c)) 2002-2016, Rice University
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

#include <string>
using std::string;

#include <cstring>
#include <sys/stat.h>
#include <dirent.h>

//*************************** User Include Files ****************************

#include <include/gcc-attr.h>

#include "FileUtil.hpp"
#include "PathFindMgr.hpp"
#include "StrUtil.hpp"

#include "diagnostics.h"
#include "pathfind.h"
#include "realpath.h"

//***************************************************************************

//***************************************************************************
// private operations
//***************************************************************************

// if there is a ".." path component in a path that is not absolute, 
// then the path may lie outside the path cache computed. if that is the
// case, inform PathFind::pathfind that it should try the slow lookup
// method, which looks outside the cache.
bool try_slow_lookup_for_relative_paths(const char *path)
{
  bool contains_relative = false;

  if (path[0] != '/') {
    contains_relative = strstr(path, "..") != NULL;
  }
  return contains_relative;
}

//***************************************************************************
// PathFindMgr
//***************************************************************************

static PathFindMgr s_singleton;

PathFindMgr::PathFindMgr()
{
  m_isPopulated = false;
  m_isFull = false;
  m_size = 0;
}


PathFindMgr::~PathFindMgr()
{
}


PathFindMgr&
PathFindMgr::singleton()
{
  return s_singleton;
}


const char*
PathFindMgr::pathfind(const char* pathList, const char* name, const char* mode)
{
  // -------------------------------------------------------
  // 0. Cache files found using 'pathList'
  // -------------------------------------------------------
  if (!m_isPopulated) {
    m_isPopulated = true;
    std::vector<std::string> pathVec; // will contain all -I paths
    StrUtil::tokenize_str(std::string(pathList), ":", pathVec);
    
    std::set<std::string> seenPaths;
    std::vector<std::string> recursionStack;
    while (!m_isFull && !pathVec.empty()) {
      if (pathVec.back() != ".") { // do not cache within CWD
	scan(pathVec.back(), seenPaths, &recursionStack);
	seenPaths.clear();
	recursionStack.clear();
      }
      pathVec.pop_back();
    }
  }

  // -------------------------------------------------------
  // 1. Resolve 'name' either by pathfind cache or by pathfind_slow
  // -------------------------------------------------------
  static std::string name_real;
  name_real = name;

  bool found = find(name_real);
 
  if (!found) {
    if (m_isFull || try_slow_lookup_for_relative_paths(name)) {
      std::set<std::string> seenPaths;
      const char* temp = pathfind_slow(pathList, name, mode, seenPaths);
      if (temp) {
        found = true;
        name_real = temp;
      }
    }
  }

  // -------------------------------------------------------
  // 2. Resolve (a) non-real-paths found by pathfind, (b) absolute
  // paths not found by pathfind() and (c) paths relative to the
  // current-working-directory.
  // -------------------------------------------------------

  name_real = RealPath(name_real.c_str());

  if (found || name_real[0] == '/') {
    return name_real.c_str();
  }
  else {
    return NULL; // failure
  }
}


const char*
PathFindMgr::pathfind_slow(const char* pathList, const char* name,
			   const char* mode,
			   std::set<std::string>& seenPaths)
{
  // -------------------------------------------------------
  // *. Collect all recursive and non-recursive paths in separate lists
  // -------------------------------------------------------

  const char* result = NULL; // 'result' will point to storage in ::pathfind

  int len = strlen(pathList) + 1;
  char* myPathList  = new char[len];
  char* pathList_nr = new char[len];
  char* pathList_r  = new char[len];
  strcpy(myPathList, pathList);
  pathList_nr[0] = '\0';
  pathList_r[0]  = '\0';
  
  char* aPath = strtok(myPathList, ":");
  int first_nr = 1;
  while (aPath != NULL) {
    if (PathFindMgr::isRecursivePath(aPath)) {
      // copy to recursive path list (Do not copy trailing '/*' )
      int l = strlen(aPath);
      strncat(pathList_r, aPath, l - RecursivePathSfxLn);
      strcat(pathList_r, ":"); // will have a trailing ':' for 'strchr'
    }
    else {
      // copy to non-recurisve path list
      if (first_nr == 0) { strcat(pathList_nr, ":"); }
      strcat(pathList_nr, aPath);
      first_nr = 0; // the first copy has been made
    }

    aPath = strtok((char*)NULL, ":");
  }
  delete[] myPathList;

  // -------------------------------------------------------
  // 1. Try a ::pathfind on all non-recursive paths
  // -------------------------------------------------------
  char* sep; // pre-declaration because of 'goto'
  result = ::pathfind(pathList_nr, name, mode);
  if (result) {
    goto fini;
  }
  
  // -------------------------------------------------------
  // 2. Try a pathfind on all recursive paths
  // -------------------------------------------------------
  // For every recursive path... (cannot use 'strtok' because of recursion)
  aPath = pathList_r;       // beginning of token string
  sep = strchr(aPath, ':'); // end of token
  while (sep != NULL) {
    *sep = '\0'; // now 'aPath' points to only the current token
    
    // 2a. Do a pathfind in this directory
    result = ::pathfind(aPath, name, mode);
    if (result) {
      goto fini;
    }
    
    // 2b. If no match, open the directory and do a pathfind
    //     on every sub-directory
    std::string myPath = aPath;
    std::string dirPathList = scan(myPath, seenPaths);
    
    if (!dirPathList.empty()) {
      result = pathfind_slow(dirPathList.c_str(), name, mode, seenPaths);
      if (result) {
	goto fini;
      }
    }
    
    *sep = ':';               // restore full token string
    aPath = sep + 1;          // points to beginning of next token or '\0'
    sep = strchr(aPath, ':'); // points to end of next token or NULL
  }
  
 fini:
  delete[] pathList_nr;
  delete[] pathList_r;
  return result;
}


bool
PathFindMgr::find(std::string& pathNm)
{
  std::string fileName = FileUtil::basename(pathNm);
  PathMap::iterator it = m_cache.find(fileName);

  if (it != m_cache.end()) {
    int levelsDeep = resolve(pathNm); // min depth a path must be
    const std::vector<std::string>& pathVec = it->second;

    // -----------------------------------------------------
    // short-circuit if only one result is possible
    // -----------------------------------------------------
    if ((pathNm.find_first_of('/') == pathNm.npos) // only filename given
	|| (pathVec.size() == 1)) { // only 1 string in pathVec
      pathNm = pathVec[0];
      return true;
    }

    // -----------------------------------------------------
    // general case
    // -----------------------------------------------------
    std::string toReturn;
    int comparisonDepth = 0;
    std::vector<std::string>::const_iterator it1;

    for (it1 = pathVec.begin(); it1 != pathVec.end(); it1++) {
      const std::string& currentPath = *it1;
      
      if (currentPath == toReturn) {
	continue;
      }
      
      size_t cTrailing = currentPath.length(); //trailing index
      // cIn points to first char after last '/'
      size_t cIn = currentPath.find_last_of("/") + 1;

      // these number will be same for all iterations, consider caching.
      size_t fpTrailing = pathNm.length();
      size_t fpIn = pathNm.find_last_of("/") + 1;
      
      int level = -1; // since the filename will always match
      int totalLevels = 0; // total levels in currentPath
      bool loopedOnce = false;
      while (cIn < cTrailing && cTrailing != currentPath.npos) {
	// checks how deep the 2 strings are congruent
	// also counts how many levels currentPath is
	if (!loopedOnce) {
	  std::string comp1 = currentPath.substr(cIn, cTrailing - cIn);
	  std::string comp2 = pathNm.substr(fpIn, fpTrailing - fpIn);

	  if (comp1 == comp2) {
	    level++;
	    // b/c the fp vars change here, comp2 only changes once
	    // the segments are equal
	    fpTrailing = fpIn - 1;
	    fpIn = pathNm.find_last_of("/", fpTrailing - 1) + 1;
	    
	    if (fpIn >= fpTrailing || fpTrailing == pathNm.npos) {
	      loopedOnce = true;
	    }
	  }
	}

	cTrailing = cIn - 1; //points to previous '/'
	// cIn points to first char after next '/'
	cIn = currentPath.find_last_of("/", cTrailing - 1) + 1;
	
	totalLevels++;
      }
      
      // only allow toReturn to change if level is greater than the current
      // comparisonDepth and if there are enough levels in currentPath to
      // satisfy levelsDeep
      if (level > comparisonDepth && totalLevels >= levelsDeep) {
	comparisonDepth = level;
	toReturn = currentPath;
      }
    }
    
    pathNm = toReturn;

    if (pathNm.empty()) {
      pathNm = pathVec[0];
    }

    return true;
  }
  
  return false;
}


void
PathFindMgr::insert(const std::string& path)
{
  std::string fnm = FileUtil::basename(path);
 
  // -------------------------------------------------------
  // Insert <fnm, path> in m_cache if there is still space
  // -------------------------------------------------------
  if (m_size < s_sizeMax) {
    PathMap::iterator it = m_cache.find(fnm);
    if (it == m_cache.end()) {
      // 0. There is no entry for 'fnm'
      std::vector<std::string> pathVec;
      pathVec.push_back(path);
      m_cache.insert(std::make_pair(fnm, pathVec));
      
      m_size += sizeof(pathVec);
      m_size += fnm.size() + 1 + path.size() + 1;
    }
    else {
      // 1. There is already an entry for 'fnm'
      std::vector<std::string>& pathVec = it->second;
      
      for (std::vector<std::string>::const_iterator it1 = pathVec.begin();
	   it1 != pathVec.end(); ++it1) {
	const std::string& x = *it1;
	if (x == path) {
	  return;
	}
      }
      pathVec.push_back(path);

      m_size += path.size() + 1;
    }
  }
  else {
    // Not smart caching. We are not expecting there to be more data
    // than can fit in the cache, but in case there is, this will
    // prevent the program from crashing.
    DIAG_WMsgIf(m_size >= s_sizeMax,
		"PathFindMgr::insert(): cache size limit reached");
    m_isFull = true;
  }
}


std::string
PathFindMgr::scan(std::string& path, std::set<std::string>& seenPaths,
		  std::vector<std::string>* recursionStack)
{
  bool doCacheFiles = (recursionStack != NULL);

  bool doRecursiveScan = isRecursivePath(path.c_str());
  if (doRecursiveScan) {
    path = path.substr(0, path.length() - RecursivePathSfxLn);
  }

  std::string localPaths;

  if (path.empty()) {
    return localPaths;
  }

  // -------------------------------------------------------
  // ensure we have not already seen 'path' (e.g., through a symlink)
  // -------------------------------------------------------

  // 0. ensure we are using a canonical path by realpath-ing (a) the
  //    root path and (b) symlink directories (below)
  if ((recursionStack && recursionStack->empty())
      || !recursionStack) {
    path = RealPath(path.c_str());
  }

  // 1. check for a past occurance
  std::set<std::string>::iterator it = seenPaths.find(path);
  if (it != seenPaths.end()) {
    return localPaths;
  }

  seenPaths.insert(path);


  // -------------------------------------------------------
  // Scan 'path'
  // -------------------------------------------------------
  DIR* dir = opendir(path.c_str());
  if (!dir) {
    return localPaths;
  }

  bool isFirstDir = true;
  struct dirent* x;
  while ( (x = readdir(dir)) ) {
    // skip "." and ".."
    if (strcmp(x->d_name, ".") == 0 || strcmp(x->d_name, "..") == 0) {
      continue;
    }
    
    std::string x_fnm = path + "/" + x->d_name;

    // --------------------------------------------------
    // compute type of 'x_fnm'
    // --------------------------------------------------
    unsigned char x_type = DT_UNKNOWN;
#if defined(_DIRENT_HAVE_D_TYPE)
    x_type = x->d_type;
#endif

    // Even if 'd_type' is available, it may be bogus.  Try stat().
    if (x_type == DT_UNKNOWN) {
      struct stat statbuf;
      int ret = lstat(x_fnm.c_str(), &statbuf); // do not follow symlinks!
      if (ret != 0) {
	continue; // error
      }
      
      if (S_ISLNK(statbuf.st_mode)) {
	x_type = DT_LNK; // S_IFLNK
      }
      else if (S_ISREG(statbuf.st_mode)) {
	x_type = DT_REG; // S_IFREG
      }
      else if (S_ISDIR(statbuf.st_mode)) {
	x_type = DT_DIR; // S_IFDIR
      }
    }

    // --------------------------------------------------
    // special case: resolve symlink to regular file or directory
    // --------------------------------------------------
    if (x_type == DT_LNK) {
      struct stat statbuf;
      int ret = stat(x_fnm.c_str(), &statbuf); // 'stat' resolves symlinks
      if (ret != 0) {
	continue; // error
      }
      
      if (S_ISREG(statbuf.st_mode)) {
	x_type = DT_REG;
      }
      else if (S_ISDIR(statbuf.st_mode)) {
	x_type = DT_DIR;
	x_fnm = RealPath(x_fnm.c_str());
	if (seenPaths.find(x_fnm) != seenPaths.end()) {
	  continue; // avoid cycles
	}
      }
    }

    // --------------------------------------------------
    // case 1: regular file
    // --------------------------------------------------
    if (x_type == DT_REG) {
      if (doCacheFiles && !m_isFull) {
	insert(x_fnm);
      }
    }

    // --------------------------------------------------
    // case 2: directory
    // --------------------------------------------------
    else if (x_type == DT_DIR) {
      if (recursionStack) {
	if (doRecursiveScan) {
	  x_fnm += "/*";
	  recursionStack->push_back(x_fnm);
	}
      }
      else {
	x_fnm += "/*";
	if (!isFirstDir) {
	  localPaths += ":";
	}
	localPaths += x_fnm;
	isFirstDir = false;
      }
    }
  }
  closedir(dir);
  
  if (recursionStack && !recursionStack->empty()) {
    std::string nextPath = recursionStack->back();
    recursionStack->pop_back();
    scan(nextPath, seenPaths, recursionStack);
  }

  return localPaths;
}


int
PathFindMgr::resolve(std::string& path)
{
  if (path[0] == '/') { // path in resolved form
    return 0;
  }
  
  std::string result;
  int trailing = path.length();
  int in = path.find_last_of("/") + 1; // add 1 to move past '/'
  int levelsBack = 0;
  
  while (trailing != -1) {
    std::string section = path.substr(in, trailing - in + 1 );
    
    if (section == "../") { // don't include it in result yet.
      levelsBack++;
    }
    else if (section != "./") { // here, section is some directory/file name
      if (levelsBack == 0) {
	result = section +  result; // append section to the beginning
      }
      else { // here, have encountered at least 1 ".." so don't include section
	levelsBack--; // since we didnt include section, we went back a level
      }
    }
    
    trailing = in - 1;
    in = path.find_last_of("/", trailing - 1) + 1;
  }
  
  if (!result.empty()) {
    path = result;
  }
  return levelsBack;
}


//-----------------------------------------------------------
// PathFindMgr::isRecursivePath
//
// Description
//   If the last two characters on a path are '/*' or '/+' 
//   then treat this as a path that needs to be recursively
//   expanded. The '+' was added as a superior alternative to
//   '*' because it sidesteps problems with quoting '*' to avoid
//   interaction with shell expansion when wrapper scripts
//   are employed.
//
// Modification history:
//   2012/03/02 - johnmc
//-----------------------------------------------------------
int 
PathFindMgr::isRecursivePath(const char* path)
{
  int l = strlen(path);
  if (l > PathFindMgr::RecursivePathSfxLn && 
      (path[l - 1] == '*' || path[l - 1] == '+') &&
      path[l - 2] == '/') {
    return 1;
  }
  return 0;
}


//***************************************************************************

string
PathFindMgr::toString(uint flags) const
{
  std::ostringstream os;
  dump(os, flags);
  return os.str();
}


std::ostream&
PathFindMgr::dump(std::ostream& os, uint GCC_ATTR_UNUSED flags,
		  const char* pfx) const
{
  using std::endl;

  os << pfx << "[ PathFindMgr: " << endl
     << pfx << "  isPopulated: " << m_isPopulated << endl
     << pfx << "  isFull: " << m_isFull << endl
     << pfx << "  size: " << m_size << " (max: " << s_sizeMax << ")" << endl;
  for (PathMap::const_iterator it = m_cache.begin();
       it != m_cache.end(); ++it) {
    const string& x = it->first;
    const std::vector<string>& y = it->second;

    os << pfx << "  " << x << " => {";
    for (size_t i = 0; i < y.size(); ++i) {
      if (i != 0) {
	os << ", ";
      }
      os << y[i];
    }
    os << "}" << endl;
  }
  os << pfx << "]" << endl;
  return os;
}


void
PathFindMgr::ddump(uint flags) const
{
  dump(std::cerr, flags);
}
