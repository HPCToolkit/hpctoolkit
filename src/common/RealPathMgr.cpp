// SPDX-FileCopyrightText: 2002-2024 Rice University
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

//************************* System Include Files ****************************

#include <string>
using std::string;


//*************************** User Include Files ****************************

#include "lean/gcc-attr.h"

#include "Logic.hpp"
#include "RealPathMgr.hpp"
#include "PathReplacementMgr.hpp"
#include "PathFindMgr.hpp"
#include "StrUtil.hpp"

#include "diagnostics.h"
#include "realpath.h"

//*************************** Forward Declarations **************************

//***************************************************************************

//***************************************************************************
// RealPathMgr
//***************************************************************************

static RealPathMgr s_singleton;


// Constructor with static singleton objects for PathFindMgr and
// PathReplacementMgr.
RealPathMgr::RealPathMgr()
{
  m_pathFindMgr = NULL;
  m_pathReplaceMgr = NULL;
}


// Constructor with params for PathFindMgr and PathReplacementMgr to
// use instead of singletons.
RealPathMgr::RealPathMgr(PathFindMgr * findMgr, PathReplacementMgr * replaceMgr)
{
  m_pathFindMgr = findMgr;
  m_pathReplaceMgr = replaceMgr;
}


// Delete path manager dependencies if non-null.
RealPathMgr::~RealPathMgr()
{
  if (m_pathFindMgr != NULL) {
    delete m_pathFindMgr;
  }
  if (m_pathReplaceMgr != NULL) {
    delete m_pathReplaceMgr;
  }
}


RealPathMgr& RealPathMgr::singleton()
{
  return s_singleton;
}


bool
RealPathMgr::realpath(string& pathNm) const
{
  if (pathNm.empty()) {
    return false;
  }

  // INVARIANT: 'pathNm' is not empty

  // INVARIANT: all entries in the map are non-empty
  MyMap::iterator it = m_cache.find(pathNm);

  // -------------------------------------------------------
  // 1. Check cache for 'pathNm'
  // -------------------------------------------------------
  if (it != m_cache.end()) {
    // use cached value
    const string& pathNm_real = it->second;
    if (pathNm_real[0] == '/') { // optimization: only copy if fully resolved
      pathNm = pathNm_real;
    }
  }
  else {
    // -------------------------------------------------------
    // 2. Consult cache with path-replaced 'pathNm'
    // -------------------------------------------------------
    string pathNm_orig = pathNm;

    if (m_pathReplaceMgr != NULL) {
      pathNm = m_pathReplaceMgr->replace(pathNm);
    }
    else {
      pathNm = PathReplacementMgr::singleton().replace(pathNm);
    }

    it = m_cache.find(pathNm);

    if (it != m_cache.end()) {
      // use cached value
      const string& pathNm_real = it->second;
      if (pathNm_real[0] == '/') { // optimization: only copy if fully resolved
        pathNm = pathNm_real;
      }

      // since 'pathNm_orig' was not in map, ensure it is
      m_cache.insert(make_pair(pathNm_orig, pathNm_real));
    }
    else {
      // -------------------------------------------------------
      // 3. Resolve 'pathNm' using PathFindMgr or realpath
      // -------------------------------------------------------
      string pathNm_real = pathNm;

      if (m_searchPaths.empty()) {
        pathNm_real = RealPath(pathNm.c_str());
      }
      else {
        const char* pathNm_pf;

        if (m_pathFindMgr != NULL) {
          pathNm_pf =
            m_pathFindMgr->pathfind(m_searchPaths.c_str(), pathNm.c_str(), "r");
        }
        else {
          pathNm_pf =
            PathFindMgr::singleton().pathfind(m_searchPaths.c_str(), pathNm.c_str(), "r");
        }
        if (pathNm_pf) {
          pathNm_real = pathNm_pf;
        }
      }

      pathNm = pathNm_real;
      m_cache.insert(make_pair(pathNm_orig, pathNm_real));
    }
  }
  return (pathNm[0] == '/'); // fully resolved
}


void
RealPathMgr::searchPaths(const string& pathsStr)
{
  std::vector<std::string> searchPathVec;
  StrUtil::tokenize_str(pathsStr, ":", searchPathVec);

  // INVARIANT: m_searchPaths is non-empty
  m_searchPaths += "."; // current working directory

  for (unsigned int i = 0; i < searchPathVec.size(); ++i) {
    string path = searchPathVec[i];

    if (PathFindMgr::isRecursivePath(path.c_str())) {
      path = path.substr(0, path.length() - PathFindMgr::RecursivePathSfxLn);
      path = RealPath(path.c_str());
      path += "/*";
    }
    else if (path != ".") {
      path = RealPath(path.c_str());
    }

    m_searchPaths += ":" + path;
  }
}


//***************************************************************************

string
RealPathMgr::toString(unsigned int flags) const
{
  std::ostringstream os;
  dump(os, flags);
  return os.str();
}


std::ostream&
RealPathMgr::dump(std::ostream& os, unsigned int GCC_ATTR_UNUSED flags,
                  const char* pfx) const
{
  os << pfx << "[ RealPathMgr:" << std::endl;
  for (MyMap::const_iterator it = m_cache.begin(); it != m_cache.end(); ++it) {
    const string& x = it->first;
    const string& y = it->second;
    os << pfx << "  " << x << " => " << y << std::endl;
  }
  os << pfx << "]" << std::endl;

  os.flush();
  return os;
}


void
RealPathMgr::ddump(unsigned int flags) const
{
  dump(std::cerr, flags);
}
