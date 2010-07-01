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
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

//*************************** User Include Files ****************************

#include "PathFindMgr.hpp"

#include "pathfind.h"

//***************************************************************************

//***************************************************************************
// PathFindMgr
//***************************************************************************

static PathFindMgr s_singleton;

PathFindMgr::PathFindMgr()
{
}


PathFindMgr::~PathFindMgr()
{
}


PathFindMgr& 
PathFindMgr::singleton()
{
  return s_singleton;
}


void 
PathFindMgr::addPath(const std::string& path)
{
  std::string fnm = getFileName(path);

  HashMap::iterator it = m_cache.find(fnm);
  if (it == m_cache.end()) {
    std::vector<std::string> pathList;
    pathList.push_back(path);
    m_cache[fnm] = pathList;
  }
  else {
    std::vector<std::string>::iterator it;
    for (it = m_cache[fnm].begin(); it != m_cache[fnm].end(); it++) {
      std::string temp = *it;
      if (temp == path) {
	return;
      }
    }
    m_cache[fnm].push_back(path);
  }
}


bool 
PathFindMgr::getRealPath(std::string& filePath) 
{
  std::string fileName = getFileName(filePath);
  HashMap::iterator it = m_cache.find(fileName);
  
  if (it !=  m_cache.end()) {
    std::vector<std::string> paths = it->second;
    
    if ((filePath[0] == '.' && filePath[1] == '/') || //ambiguous './' path case
	(filePath.find_first_of('/') == filePath.npos)) { //only filename given
      
      filePath = paths[0];
      return true;
    }
    
    std::string toReturn;
    int comparisonDepth = -1;
    std::vector<std::string>::iterator it;

    for (it = paths.begin(); it != paths.end(); it++) {
      std::string currentPath = *it;
      
      if (currentPath == toReturn)
	continue;
      
      size_t currentEIn = currentPath.find_last_of('/');
      //add 1 so that the first '/' will NOT be included
      //won't have to worrry about currentBIn == npos since npos + 1 = 0
      size_t currentBIn = currentPath.find_last_of('/', currentEIn - 1) + 1;
      if (currentBIn > currentEIn)
	currentEIn = currentPath.length();
      size_t sizeA = currentEIn - currentBIn;
      
      
      //these numbers will be the same for every iteration...should consider
      //caching the values for the filePath string.
      size_t fileEIn = filePath.find_last_of('/');
      size_t fileBIn = filePath.find_last_of('/', fileEIn - 1) + 1;
      if (fileBIn > fileEIn)
	fileEIn = filePath.length();
      size_t sizeB = fileEIn - fileBIn;
      
      bool congruent = true;
      int level = -1;
      while (congruent && currentBIn != currentPath.npos &&
	     fileBIn != filePath.npos) { 
	//checks how deep the 2 strings are congruent
	std::string comp1 = currentPath.substr(currentBIn, sizeA);
	std::string comp2 = filePath.substr(fileBIn, sizeB);
	
	if (comp1 == comp2) {
	  level++;
	  currentEIn = currentPath.find_last_of('/', currentBIn - 1);
	  currentBIn = currentPath.find_last_of('/', currentEIn - 1) + 1;
	  sizeA = currentEIn - currentBIn;
	  
	  fileEIn = filePath.find_last_of('/', fileBIn -  1);
	  fileBIn = filePath.find_last_of('/', fileEIn - 1) + 1;
	  sizeB = fileEIn - fileBIn;
	}
	else {
	  congruent = false;
	}
      }
      
      if (level > comparisonDepth) {
	comparisonDepth = level;
	toReturn = currentPath;
      }
    }
    
    if (comparisonDepth == -1) { // if nothing matches beyond the file name
      return false; 
    }
    
    filePath = toReturn;
    return true;
  }
  
  return false; 
}


char*
PathFindMgr::pathfind_r(const char* pathList,
	   const char* name,
	   const char* mode)
{
  char* result = NULL; /* 'result' will point to storage in 'pathfind' */

  /* 0. Collect all recursive and non-recursive paths in separate lists */

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
    if (PathFindMgr::is_recursive_path(aPath)) {
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

  /* 1. Try a pathfind on all non-recursive paths */
  char* sep; // pre-declaration because of 'goto'
  result = pathfind(pathList_nr, name, mode);
  if (result) { goto pathfind_r_fini; }
  
  /* 2. Try a pathfind on all recursive paths */
  /* For every recursive path... (cannot use 'strtok' because of recursion) */
  aPath = pathList_r;       // beginning of token string
  sep = strchr(aPath, ':'); // end of token
  while (sep != NULL) {
    *sep = '\0'; // now 'aPath' points to only the current token
    
    /* 2a. Do a pathfind in this directory */
    result = pathfind(aPath, name, mode);
    if (result) { goto pathfind_r_fini; }
    
    /* 2b. If no match, open the directory and do a pathfind_r */
    /*     on every sub-directory */
    std::string dirPathList = subdirs_to_pathlist(aPath, 1 /*recursive*/);
    if (!dirPathList.empty()) {
      result = pathfind_r(dirPathList.c_str(), name, mode);
      if (result) { goto pathfind_r_fini; }
    }

    *sep = ':';               // restore full token string
    aPath = sep + 1;          // points to beginning of next token or '\0'
    sep = strchr(aPath, ':'); // points to end of next token or NULL
  }
  
 pathfind_r_fini:
  delete[] pathList_nr;
  delete[] pathList_r;
  return result;
}


std::string
PathFindMgr::subdirs_to_pathlist(const char* path, int recursive)
{
  std::string resultPath;

  //this path has already been searched, so no point going over it again
  if(directoriesSearched.find(path) != directoriesSearched.end())
    return resultPath;
 
  DIR* dir = opendir(path);
  if (!dir) {
    return resultPath; // empty string
  }
  
  bool isFirst = true;
  
  struct dirent* dire;
  while( (dire = readdir(dir)) ) {
    // skip ".", ".."
    if (strcmp(dire->d_name, ".") == 0) { continue; }
    if (strcmp(dire->d_name, "..") == 0) { continue; }	
    /* if (dire->d_name[0] == '.') { continue; } hidden files/directories */
    
    // add directories
    std::string file = std::string(path) + "/" + dire->d_name;
    bool searched = directoriesSearched.find(file) != 
      directoriesSearched.end();
    
    //only search/add to PathFindMgr if the subdir hasn't already been searched
    if (!searched && dire->d_type == DT_DIR) {
      if (!isFirst) {
	resultPath += ":";
      }
      if (recursive > 0) {
	file += "/*";
      }
      resultPath += file;
      isFirst = false;
    } 
    else if(!searched && dire->d_type == DT_REG) { //cache files
      PathFindMgr::singleton().addPath(file);
    }
  }
  closedir(dir);
  
  //if resultPath is empty, means that all subdirectories of path have been
  //searched, so we can add it into the list of directories fully searched.
  if(resultPath.empty())
    directoriesSearched.insert(path);
  
  return resultPath;
}


std::string
PathFindMgr::getFileName(const std::string& path) const
{
  size_t in = path.find_last_of("/");
  if (in != path.npos && path.length() > 1)
    return path.substr(in + 1);
  
  return path;
}


int 
PathFindMgr::is_recursive_path(const char* path)
{
  int l = strlen(path);
  if (l > PathFindMgr::RECURSIVE_PATH_SUFFIX_LN && path[l - 1] == '*' &&
      path[l - 2] == '/') { 
    return 1;
  }
  return 0;
}
