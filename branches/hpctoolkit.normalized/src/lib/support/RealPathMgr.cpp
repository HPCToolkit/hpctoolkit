// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://outreach.scidac.gov/svn/hpctoolkit/trunk/src/lib/support/RealPathMgr.cpp $
// $Id: RealPathMgr.cpp 2962 2010-07-21 20:30:36Z gvs1 $
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
//   $HeadURL: https://outreach.scidac.gov/svn/hpctoolkit/trunk/src/lib/support/RealPathMgr.cpp $
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

#include "Logic.hpp"
#include "RealPathMgr.hpp"
#include "PathReplacementMgr.hpp"
#include "PathFindMgr.hpp"

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
RealPathMgr::realpath(string& pathNm)
{
  if (pathNm.empty()) {
    return false;
  }
  
  // INVARIANT: 'pathNm' is not empty

  // INVARIANT: all entries in the map are non-empty
  MyMap::iterator it = m_realpath_map.find(pathNm);

  // -------------------------------------------------------
  // 1. Check cache for 'pathNm'
  // -------------------------------------------------------
  if (it != m_realpath_map.end()) {
    // use cached value
    const string& pathNm_real = it->second;
    if (pathNm_real[0] == '/') { // optimization to avoid copy
      pathNm = pathNm_real;
    }
  }
  else {
    // -------------------------------------------------------
    // 2. Consult cache with path-replaced 'pathNm'
    // -------------------------------------------------------
    pathNm = PathReplacementMgr::singleton().getReplacedPath(pathNm);
    it = m_realpath_map.find(pathNm);
    if (it != m_realpath_map.end()) {
      // use cached value
      const string& pathNm_real = it->second;
      if (pathNm_real[0] == '/') { // optimization to avoid copy
	pathNm = pathNm_real;
      }
    }
    else {
      // -------------------------------------------------------
      // 3. Resolve 'pathNm' using PathFindMgr or realpath
      // -------------------------------------------------------
      string pathNm_orig = pathNm;
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
      m_realpath_map.insert(make_pair(pathNm_orig, pathNm_real));
    }
  }
  return (pathNm[0] == '/'); // fully resolved
}


void
RealPathMgr::searchPaths(const string& sPaths)
{
  size_t trailingIn = -1;
  size_t in = sPaths.find_first_of(":");
  
  while (trailingIn != sPaths.length()) {
    //since trailingIn points to a ":", must add 1 to point to the path
    trailingIn++;
    std::string currentPath = sPaths.substr(trailingIn, in - trailingIn);

    if (PathFindMgr::isRecursivePath(currentPath.c_str())) {
      //if its recursive, need to strip off and add back on '/*'
      currentPath = currentPath.substr(0,currentPath.length() - 2);
      currentPath = RealPath(currentPath.c_str());
      m_searchPaths+= (currentPath + "/*:");
    }
    else if(currentPath != ".") { //so we can exclude this from cache
      currentPath = RealPath(currentPath.c_str());
      m_searchPaths+= (currentPath + ":");
    }
    
    trailingIn = in;
    in = sPaths.find_first_of(":",trailingIn + 1);
    
    if (in == sPaths.npos) { //deals with corner case of last element
      in = sPaths.length();
    }
  }
  m_searchPaths += "."; //add CWD back in
}


//***************************************************************************

string
RealPathMgr::toString(int flags) const
{
  return "";
}


std::ostream&
RealPathMgr::dump(std::ostream& os, int flags) const
{
  os.flush();
  return os;
}


void
RealPathMgr::ddump(int flags) const
{
  dump(std::cerr, flags);
}

