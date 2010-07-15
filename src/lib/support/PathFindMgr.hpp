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

#ifndef PathFindMgr_hpp
#define PathFindMgr_hpp

//************************* System Include Files ****************************

#include <map>
#include <set>
#include <string>
#include <vector>

//*************************** User Include Files ****************************

//*************************** Forward Declarations **************************

//***************************************************************************
// PathFindMgr
//***************************************************************************

class PathFindMgr
{
public:
  static const int RECURSIVE_PATH_SUFFIX_LN  = 2;


public:
  PathFindMgr();
  ~PathFindMgr();
  
  
  static PathFindMgr&
  singleton();
  
  
  // Retreives the highest priority and closest matching real path to
  // "filePath" from 'm_cache', where priority is defined by how close
  // the real path is to the front of the vector of real paths.  If
  // the file name exists in 'm_cache', but none of the real paths
  // match it beyond the file name, the first path in the vector will
  // be returned.
  // 
  // Notes:
  // * We cache all files in a recursive seach path such that:
  //   - the path portion of the file is fully resolved
  //   - the filename portion may or may not be a symlink
  //   - with forward-sym links, there can be multiple paths that lead to
  //     the same file.  We only cache the real-pathed path + the file
  //
  // * Will not go into an infinite loop when symlinks form a loop.
  //
  // * Ambiguous case 1:
  //     input: ../../zoo.c
  //     cache for entry 'zoo.c'
  //       0. p0/p1/zoo.c
  //       1. p0/p2/zoo.c
  //       2. p3/p4/zoo.c
  //
  //   In this case the input matches all three entries.  We return
  //   entry 0.
  //
  // * Ambiguous case 2:
  //     input: ../p5/zoo.c
  //     cache: 
  //       0. p0/p1/zoo.c
  //       1. p0/p2/zoo.c
  //       2. p3/p4/zoo.c
  //
  //   In this case, the input matches no entry.  However, because p5
  //   could be a symlink, it could potentially match one or *or* no
  //   entry.  In this case we still return entry 0.
  //
  // @param filePath: A partial path to the file we are searching for. Will be
  //                  altered to be its associated path.
  //       *note* - 'filePath' can range from a file name to a full path
  //                            
  // @return:  A bool indicating whether 'filePath' was found in the list.
  //
  bool
  getRealPath(std::string& filePath);
  
  
  //pathfind_r - (recursively) search for named file in given
  //---------- colon-separated (recursive) pathlist 
  //
  //First searches for 'name' in the PathFindMgr::singleton member
  //'m_cache'.  If that search is unsuccessful and the cache is full,
  //it then searches for a file named "name" in each directory in the
  //colon-separated pathlist given as the first argument, and returns
  //the full pathname to the first occurence that has at least the
  //mode bits specified by mode. For any 'recursive-path', it
  //recursively searches all of that paths descendents as well. An
  //empty path in the pathlist is interpreted as the current
  //directory.  Returns NULL if 'name' is not found.
  //
  //A 'recursive-path' is specified by appending a single '*' at the
  //end of the directory. /home/.../dir/\*
  //
  // ** Note: the '*' is escaped with '\' so it does not look like a
  //C-style comment; in reality it should not be escaped! **
  //   
  //The following mode bits are understood: 
  //    "r" - read access
  //    "w" - write access
  //    "x" - execute access
  //
  //The returned pointer points to an area that will be reused on subsequent
  //calls to this function, and must not be freed by the caller.
  char*
  pathfind_r(const char* pathList,
	     const char* name,
	     const char* mode);
  
  
  //Is this a valid recursive path of the form '.../path/\*' ? 
  static int 
  is_recursive_path(const char* path);
  

private:

  //This method adds a file name and its associated real path to 'm_cache'.
  //Paths are store according to the file it is associated with. Path is not
  //added if it is already in the vector associated with the file name.
  //
  //@param realPath: The path a file is located at.
  void 
  addPath(const std::string& path);
  

  //Private helper method that returns the file name associated with 'path', 
  //which is assumed to be the part of 'path' after the last "/".
  //
  //@param path: The full or partial path whose associated file name
  //             is wanted.  
  //
  //@return: If a "/" is in 'path', the portion of 'path' after the
  //         last "/", otherwise 'path' is returned.
  std::string 
  getFileName(const std::string& path) const;


  //Private helper method that recursively searches through all the 
  //directories in 'pathList' and caches all the files in those directories.
  //Should be used only once.
  //
  //@param pathList: List of directories that contain files to be cached.
  //                 Each path is separated by a ":" and recursive paths have
  //                 a '/*' at the end of the path.
  //@param recursive: Indicates whether a directory should be recursively
  //                  searched. 
  void
  fill(const std::string& pathList, bool recursive);
  
  //Resolves all '..' and '.' in 'path' in reference to itself. Does
  //NOT find the unique real path of 'path'. Returns how many '..'
  //are left in 'path'. Helps make sure more accurate results are
  //returned from getRealPath().
  // 
  //Ex: if path = 'src/../../lib/src/./../ex.c' then resolve(path) would turn
  //    path into '../lib/ex.c'.
  //
  //@param path: The file path to resolve.
  //@return:     The number of '..' in 'path' after it has been resolved.
  int 
  resolve(std::string& path);
  
  
  //Converts all subdirectories of 'path' to a valid pathlist. If
  //'recursive' is true, then all paths are made recursive
  std::string
  subdirs_to_pathlist(const std::string& path, bool isRecursive);


private: 
  typedef
  std::map<std::string, std::vector<std::string> > PathMap;

  PathMap m_cache;
  std::map<std::string, bool> directories;
  bool m_cacheNotFull;
  bool m_filled;

  static const uint64_t s_sizeLimit = 20 * 1024 * 1024; //default is 20 MB
  uint64_t m_size;
};

#endif
