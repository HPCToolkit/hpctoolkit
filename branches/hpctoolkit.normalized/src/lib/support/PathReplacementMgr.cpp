// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://outreach.scidac.gov/svn/hpctoolkit/trunk/src/lib/support/PathReplacementMgr.cpp $
// $Id: PathReplacementMgr.cpp 2941 2010-07-15 19:18:34Z gvs1 $
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
//   $HeadURL: https://outreach.scidac.gov/svn/hpctoolkit/trunk/src/lib/support/PathReplacementMgr.cpp $
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

//************************* System Include Files ****************************

#include <algorithm>
#include <cstring>

//*************************** User Include Files ****************************

#include "PathReplacementMgr.hpp"

//*************************** Forward Declarations **************************

//***************************************************************************

//***************************************************************************
// PathReplacementMgr
//***************************************************************************

static PathReplacementMgr s_singleton;

PathReplacementMgr::PathReplacementMgr()
{
}


PathReplacementMgr::~PathReplacementMgr()
{
}

PathReplacementMgr&
PathReplacementMgr::singleton()
{
  return s_singleton;
}


std::string
PathReplacementMgr::getReplacedPath(const std::string& original)
{
  for (size_t i = 0; i < m_pathReplacement.size(); i++) {
    StringPair temp = m_pathReplacement[i];
    size_t found = strncmp(original.c_str(), temp.first.c_str(),
			   temp.first.length());
    
    if (found == 0) {
      std::string newPath = original;
      newPath.replace(0, temp.first.size(), temp.second);
      return newPath;
    }
  }
  return original;
}


//Comparison function we use to sort 'pathReplacment' according to the first 
//values of the pair, which is the old partial path.
//
//@param a: The first pair object to compare.
//@param b: The second pair object to compare.
//return:   A bool indicatin if a.first.size() < b.first.size().
static bool
compare_as_strings(const PathReplacementMgr::StringPair& a,
		   const PathReplacementMgr::StringPair& b)
{
  return a.first.size() > b.first.size();
}


void
PathReplacementMgr::addPath(const std::string& originalPath,
			    const std::string& newPath)
{
  StringPair temp(originalPath, newPath);
  m_pathReplacement.push_back(temp);
  stable_sort(m_pathReplacement.begin(), m_pathReplacement.end(),
	      compare_as_strings);
}



