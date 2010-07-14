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
RealPathMgr::realpath(string& fnm)
{
  if (fnm.empty()) {
    return false;
  }
  
  // INVARIANT: 'fnm' is not empty

  // INVARIANT: all entries in the map are non-empty
  MyMap::iterator it = m_realpath_map.find(fnm);

  if (it != m_realpath_map.end()) {
    // use cached value
    const string& fnm_real = it->second;
    if (fnm_real[0] == '/') { // only copy if realpath was found
      fnm = fnm_real;
    }
  }
  else {
    fnm = PathReplacementMgr::singleton().getReplacedPath(fnm);
    it = m_realpath_map.find(fnm);
    if (it != m_realpath_map.end()) { //check if modified fnm is cached 
      const string& fnm_real = it->second;
      if (fnm_real[0] == '/') {
	fnm = fnm_real;
      }
    }
    else {
      string fnm_real;
      
      const char* pf = fnm.c_str();
      if (!m_searchPaths.empty()) {
	pf = PathFindMgr::singleton().pathfind_r(m_searchPaths.c_str(), 
						 fnm.c_str(), "r");
      }
      if (pf) { 
	fnm_real = pf;
	m_realpath_map.insert(make_pair(fnm, fnm_real));
	fnm = fnm_real;
      }
      else {
	// 'pf' is NULL iff pathfind_r failed -- RealPath won't do any better
	fnm_real = fnm;
	m_realpath_map.insert(make_pair(fnm, fnm_real));
      }
    }
  }
  return (fnm[0] == '/'); // fully resolved
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

    if (PathFindMgr::is_recursive_path(currentPath.c_str())) {
      //if its recursive, need to strip off and add back on '/*'
      currentPath = currentPath.substr(0,currentPath.length() - 2);
      currentPath = RealPath(currentPath.c_str());
      m_searchPaths+= (currentPath + "/*:");
    }
    else {
      currentPath = RealPath(currentPath.c_str());
      m_searchPaths+= (currentPath + ":");
    }
    
    trailingIn = in;
    in = sPaths.find_first_of(":",trailingIn + 1);
    
    if (in == sPaths.npos) { //deals with corner case of last element
      in = sPaths.length();
    }
  }
  //to strip off the trailing ":"
  m_searchPaths = m_searchPaths.substr(0, m_searchPaths.length() - 1);
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

