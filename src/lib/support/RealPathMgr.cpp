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


//*************************** User Include Files ****************************

#include <include/gcc-attr.h>

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

RealPathMgr::RealPathMgr()
{
}


RealPathMgr::~RealPathMgr()
{
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

    pathNm = PathReplacementMgr::singleton().replace(pathNm);
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
	const char* pathNm_pf =
	  PathFindMgr::singleton().pathfind(m_searchPaths.c_str(),
					    pathNm.c_str(), "r");
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
  
  for (uint i = 0; i < searchPathVec.size(); ++i) {
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
RealPathMgr::toString(uint flags) const
{
  std::ostringstream os;
  dump(os, flags);
  return os.str();
}


std::ostream&
RealPathMgr::dump(std::ostream& os, uint GCC_ATTR_UNUSED flags,
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
RealPathMgr::ddump(uint flags) const
{
  dump(std::cerr, flags);
}

