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

//************************** System Include Files ***************************

#ifdef NO_STD_CHEADERS
# include <string.h>
#else
# include <cstring>
using namespace std; // For compatibility with non-std C headers
#endif

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include <string> // for 'subdirs_to_pathlist'

//*************************** User Include Files ****************************

#include "pathfind.h"
#include "CStrUtil.h" // for 'ssave' and 'sfree'

//*************************** Forward Declarations ***************************

//***************************************************************************

std::string
subdirs_to_pathlist(const char* path, int recursive);


int 
is_recursive_path(const char* path)
{
  int l = strlen(path);
  if (l > RECURSIVE_PATH_SUFFIX_LN && path[l - 1] == '*' &&
      path[l - 2] == '/') { 
    return 1;
  }
  return 0;
}


char*
pathfind(const char* pathList,
	 const char* name,
	 const char* mode)
{
  static char* result = NULL;
  int   accessFlags = 0;
  const char* path;
  const char* sep;
  
  if (result) { sfree(result); result = NULL; }

  /* Check that name is plausible. */
  if (name == NULL || *name == '\0')
    return NULL;
  
  /* Convert mode chars to accessFlags, for use by access function. */
  if (mode) {
    if (strchr( mode, 'r' )) accessFlags |= R_OK;
    if (strchr( mode, 'w' )) accessFlags |= W_OK;
    if (strchr( mode, 'x' )) accessFlags |= X_OK;
  }
  if (!accessFlags) { accessFlags = F_OK; }
  
  /* If pathList is empty or name is absolute, don't search path for it. */
  if (pathList == NULL || *pathList == '\0' || *name == '/') {
    if (access( name, accessFlags ) >= 0) {
      result = ssave(name);
    }
    return result;
  }

  path = pathList;
  while (path) {
    
    int len, pathLen, retval;
    char* tmp;

    sep = strchr (path, ':');
    pathLen = sep ? sep - path : strlen (path);
    len = strlen (name) + 1;
    if (pathLen) len += pathLen + 1;
    tmp = new char[len];
    if (pathLen) {
      strncpy (tmp, path, pathLen);
      tmp[pathLen] = '/';    // Can't use strcat, since tmp may not
      tmp[pathLen+1] = '\0'; // be NULL-terminated following strncpy.
      strcat (tmp, name);
    }
    else {
      strcpy (tmp, name);
    }
    
    retval = access( tmp, accessFlags );
#ifdef SELFTEST
    printf ("Access %s == %d\n", tmp, retval);
#endif
    if (retval >= 0) {
      result = tmp;
      return (result);
    }
    else {
      delete[] tmp;
    }
    
    path = sep ? sep + 1 : NULL;
  }
  
  return NULL;
}


char*
pathfind_r(const char* pathList,
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
    if (is_recursive_path(aPath)) {
      // copy to recursive path list (do not copy trailing '/*')
      int l = strlen(aPath);
      strncat(pathList_r, aPath, l - RECURSIVE_PATH_SUFFIX_LN);
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

/****************************************************************************/

/*  converts all subdirectories of 'path' to a valid pathlist.  If
 *  'recursive' is 1, then all paths are made recursive
 */
std::string
subdirs_to_pathlist(const char* path, int recursive)
{
  std::string resultPath;
 
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
    if (dire->d_type == DT_DIR) {
      if (!isFirst) {
	resultPath += ":";
      }
      if (recursive > 0) {
	file += "/*";
      }
      resultPath += file;
      isFirst = false;
    }
  }
  closedir(dir);

  return resultPath;
}

/****************************************************************************/

#ifdef SELFTEST
int
main (int argc, const char *argv[])
{
  int	i;
  char*	res;
  
  for (i = 1; i < argc; i++) {
    res = pathfind (getenv ("PATH"), argv[i], "r");
    printf("findpath(%s) == %s\n", argv[i], res ? res : "<<NULL>>");
  }
}
#endif
