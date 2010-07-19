// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2010, Rice University 
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

#include <cstring>
#include <sys/stat.h>
#include <dirent.h>

//*************************** User Include Files ****************************

#include "PathFindMgr.hpp"

#include "diagnostics.h"
#include "pathfind.h"
#include "realpath.h"
#include "FileUtil.hpp"

//***************************************************************************

//***************************************************************************
// PathFindMgr
//***************************************************************************

static PathFindMgr s_singleton;

PathFindMgr::PathFindMgr()
{
  m_filled = false;
  m_cacheNotFull = true;
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
PathFindMgr::pathfind(const char* pathList,
		      const char* name,
		      const char* mode)
{
  // -------------------------------------------------------
  // 0. Cache files found using 'pathList'
  // -------------------------------------------------------
  if (!m_filled) {
    m_filled = true;
    std::string myPathList = pathList;
    fill(myPathList, false); // FIXME
  }

  // -------------------------------------------------------
  // 1. Resolve 'name' either by pathfind cache or by pathfind_slow
  // -------------------------------------------------------
  static std::string name_real;
  name_real = name;

  bool found = false;
  if (PathFindMgr::find(name_real)) {
    found = true;
  }

  if (!found && !m_cacheNotFull) {
    // FIXME: pathfind_slow();
    // if found, found = true;
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


#if 0
  // -------------------------------------------------------
  // pathfind_slow
  // -------------------------------------------------------

  // tallent: FIXME: turn into a separate pathfind_slow routine (no
  // need to repeatedly search the cache for something that isn't
  // there.

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
      // copy to recursive path list (do not copy trailing '/*')
      int l = strlen(aPath);
      strncat(pathList_r, aPath, l - PathFindMgr::RECURSIVE_PATH_SUFFIX_LN);
      strcat(pathList_r, ":"); // will have a trailing ':' for 'strchr'
    } else {
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
    if (!m_cacheNotFull) { //if we weren't able to cache everything
      std::string dirPathList = subdirs_to_pathlist(aPath,true); //search disk
      
      if (!dirPathList.empty()) {
	result = PathFindMgr::pathfind(dirPathList.c_str(), name, mode);
	if (result) {
	  goto fini;
	}
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
#endif
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
    std::vector<std::string>::const_iterator it;

    for (it = pathVec.begin(); it != pathVec.end(); it++) {
      const std::string& currentPath = *it;
      
      if (currentPath == toReturn) {
	continue;
      }
      
      size_t cTrailing = currentPath.length(); //trailing index
      // cIn points to first char after last '/'
      size_t cIn = currentPath.find_last_of("/") + 1; 

      // these number will be same for all iterations, consider caching.
      size_t fpTrailing = pathNm.length();
      size_t fpIn = pathNm.find_last_of("/") + 1;
      
      int level = -1; //since the filename will always match
      int totalLevels = 0; //total levels in currentPath
      bool loopedOnce = false;
      while (cIn < cTrailing && cTrailing != currentPath.npos) {
	// checks how deep the 2 strings are congruent 
	// also counts how many levels currentPath is
	if (!loopedOnce) {
	  std::string comp1 = currentPath.substr(cIn, cTrailing - cIn);
	  std::string comp2 = pathNm.substr(fpIn, fpTrailing - fpIn);

	  if (comp1 == comp2) {
	    level++;
	    //b/c the fp vars change here, comp2 only changes once the segments
	    //are equal
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
 
  if (m_size < s_sizeLimit) { //if under cache limit
    PathMap::iterator it = m_cache.find(fnm);

    if (it == m_cache.end()) {
      std::vector<std::string> pathList;
      pathList.push_back(path);
      m_cache[fnm] = pathList;
      
      m_size += sizeof(pathList);
      m_size += fnm.size() + 1;
    }
    else {
      std::vector<std::string>::iterator it;
      for (it = m_cache[fnm].begin(); it != m_cache[fnm].end(); it++) {
	std::string temp = *it;
	if (temp == path) { //make sure no duplicates
	  return;
	}
      }
      m_size += path.size() + 1;
      m_cache[fnm].push_back(path);
    }
  }
  else {
    //Not smart caching. We are not expecting there to be more data than can
    //fit in the cache, but in case there is, this will prevent the program
    //from crashing. 
    
    DIAG_WMsgIf(m_size >= s_sizeLimit,
		"PathFindMgr::insert(): cache size limit reached");
    m_cacheNotFull = false;
  }
}


void
PathFindMgr::fill(const std::string& myPathList, bool isRecursive)
{
  // FIXME: make 'directories' an argument

  size_t trailingIn = -1;
  size_t in = myPathList.find_first_of(":");
  
  if (in == myPathList.npos) { //pathList is a single path.
    std::string path;
    if (isRecursive) {
      path = myPathList.substr(0, myPathList.length() - 
			       RECURSIVE_PATH_SUFFIX_LN);
    }
    else {
      path = myPathList;
    }
    
    std::map<std::string, bool>::iterator it = directories.find(path);
    // if cache is full or if this directory is referenced by a symlink and we
    // have already looked at it, don't bother continuing 
    if (!m_cacheNotFull || (it != directories.end() && (it->second))) {
      return;
    }
    
    if (it != directories.end()) { //if this dire was reference by a symlink
      it->second = true;        //set it to true in the map
    }
    else {
      directories.insert(make_pair(path, true)); //don't search this path again
    }

    std::string resultPath;

    DIR* dir = opendir(path.c_str());
    if (!dir) {
      return;
    }
    
    bool isFirst = true;
    
    struct dirent* dire;
    while ( (dire = readdir(dir)) ) {
      //skip ".", ".."
      if (strcmp(dire->d_name, ".") == 0) { continue; }
      if (strcmp(dire->d_name, "..") == 0) { continue; }

      //is either a subdir to be recursed on or a file to cache
      std::string file = path + "/" + dire->d_name;
      
      if (dire->d_type == DT_LNK) { //if its a file symlink, add to singleton
	struct stat buf;
	if ( stat(file.c_str(), &buf) + 1) {
	  if (S_ISREG(buf.st_mode)) {
	    PathFindMgr::insert(file);
	    continue;
	  }
	}
      }
      
      if (isRecursive && (dire->d_type == DT_DIR || dire->d_type == DT_LNK)) {
	if (dire->d_type == DT_LNK) {
	  file = RealPath(file.c_str()); // search resolved dir only
	  if (directories.find(file) != directories.end()) {
	    continue; // check if the resolved file has already been searched
	  }
	  directories.insert(make_pair(file, false)); 
	}
	
	if (!isFirst) {
	  resultPath += ":";
	}
	
	file += "/*";
	resultPath += file;
	isFirst = false;
      }
      else if (dire->d_type == DT_REG && m_cacheNotFull) {
	PathFindMgr::insert(file);
      }
    }
    closedir(dir);
    
    if (isRecursive && !resultPath.empty()) { 
      fill(resultPath, isRecursive);
    }
    
    return;
  }
  else {
    while (trailingIn != myPathList.length() && m_cacheNotFull) {
      //since trailingIn points to a ":", must add 1 to point to the path
      std::string currentPath = myPathList.substr(trailingIn + 1,
						  in - trailingIn - 1);
         
      if (isRecursivePath(currentPath.c_str())) {
	fill(currentPath, true);
      }
      else { //non-recursive path
	fill(currentPath, false);
      }
     
      trailingIn = in;
      in = myPathList.find_first_of(":",trailingIn + 1);
      
      if (in == myPathList.npos) { //deals with corner case of last element
	in = myPathList.length();
      }
    }
  }
}


std::string
PathFindMgr::subdirs_to_pathlist(const std::string& path, bool isRecursive)
{
  std::string resultPath;

  DIR* dir = opendir(path.c_str());
  if (!dir) {
    return resultPath; // empty string
  }
  
  bool isFirst = true;
  
  struct dirent* dire;
  while ( (dire = readdir(dir)) ) {
    //skip ".", ".."
    if (strcmp(dire->d_name, ".") == 0) { continue; }
    if (strcmp(dire->d_name, "..") == 0) { continue; }
    
    //is either a subdir to be recursed on or a file to cache
    std::string file = path + "/" + dire->d_name;
    
    if (dire->d_type == DT_DIR || dire->d_type == DT_LNK) {
       if (dire->d_type == DT_LNK) {
	file = RealPath(file.c_str());
       }
      
      if (!isFirst) {
	resultPath += ":";
      }
      
      
      file += "/*";
      resultPath += file;
      isFirst = false;
    }
  }
  closedir(dir);
  
  return resultPath;
} 


int
PathFindMgr::resolve(std::string& path)
{
  if (path[0] == '/') { //path in resolved form
    return 0;
  }
  
  std::string result;
  int trailing = path.length();
  int in = path.find_last_of("/") + 1; //add 1 to move past '/'
  int levelsBack = 0;
  
  while (trailing != -1) {
    std::string section = path.substr(in, trailing - in + 1 );
    
    if (section == "../") { //don't include it in result yet.
      levelsBack++;
    }
    else if (section != "./") { //here, section is some directory/file name
      if (levelsBack == 0) {
	result = section +  result; //append section to the beginning of result
      }
      else { //here, have encountered at least 1 ".." so don't include section
	levelsBack--; //since we didnt include section, we went back a level
      } 
    }
    else if (section == "./" && in == 0) { //case of './' at start of string
      result = section + result;
    }
    
    trailing = in - 1;
    in = path.find_last_of("/", trailing - 1) + 1;
  }
  
  if (!result.empty()) {
    path = result;
  }
  return levelsBack;
}


int 
PathFindMgr::isRecursivePath(const char* path)
{
  int l = strlen(path);
  if (l > PathFindMgr::RECURSIVE_PATH_SUFFIX_LN && path[l - 1] == '*' &&
      path[l - 2] == '/') { 
    return 1;
  }
  return 0;
}


std::ostream& 
PathFindMgr::dump(std::ostream& os, uint oFlags)
{
  std::map<std::string, std::vector<std::string> >::iterator it;
  for (it = m_cache.begin(); it != m_cache.end(); it++) {
    os << "File name ==> " << it->first << "\nAssociated paths:" << std::endl;
    const std::vector<std::string>& paths = it->second;
    
    for (size_t in = 0; in < paths.size(); in++) {
      os << in << ") " << paths[in] << std::endl;
    }
    os << std::endl;
  }
  return os;
}


void 
PathFindMgr::ddump()
{
  dump(std::cerr);
}

