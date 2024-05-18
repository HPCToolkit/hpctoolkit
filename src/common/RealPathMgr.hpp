// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

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

#ifndef RealPathMgr_hpp
#define RealPathMgr_hpp

//************************* System Include Files ****************************

#include <string>
#include <map>
#include <iostream>

#include <cctype>

//*************************** User Include Files ****************************

#include "PathFindMgr.hpp"
#include "PathReplacementMgr.hpp"

//*************************** Forward Declarations **************************

//***************************************************************************
// RealPathMgr
//***************************************************************************


// --------------------------------------------------------------------------
// 'RealPathMgr'
// --------------------------------------------------------------------------

class RealPathMgr {
public:
  RealPathMgr();
  ~RealPathMgr();

  RealPathMgr(PathFindMgr *, PathReplacementMgr *);

  static RealPathMgr&
  singleton();

  // -------------------------------------------------------
  //
  // -------------------------------------------------------

  // realpath: Given 'fnm', convert it to its 'realpath' (if possible)
  // and return true.  Return true if 'fnm' is as fully resolved as it
  // can be (which does not necessarily mean it exists); otherwise
  // return false.
  bool
  realpath(std::string& pathNm) const;


  const std::string&
  searchPaths() const
  { return m_searchPaths; }


  // Given 'sPaths', each individual search path is ripped from the
  // string and has RealPath() applied to it before it is added to
  // 'm_searchPaths'. Recursive and non-recursive properties of each
  // search path is preserved.
  //
  // @param sPaths: A string of search paths with each path having recursive
  //                (meaning a '/*' is tacked on the end) or non-recursive
  //                properties and paths are separated by a ":"
  //
  void
  searchPaths(const std::string& sPaths);


  // -------------------------------------------------------
  // debugging
  // -------------------------------------------------------
  std::string
  toString(unsigned int flags = 0) const;

  // flags = -1: compressed dump / 0: normal dump / 1: extra info
  std::ostream&
  dump(std::ostream& os, unsigned int flags = 0, const char* pfx = "") const;

  void
  ddump(unsigned int flags = 0) const;


private:
  typedef std::map<std::string, std::string> MyMap;

  PathFindMgr * m_pathFindMgr;
  PathReplacementMgr * m_pathReplaceMgr;

  std::string m_searchPaths;
  mutable MyMap m_cache;
};


//***************************************************************************

#endif // RealPathMgr_hpp
