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

#include <algorithm>
#include <string>
using std::string;

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


string
PathReplacementMgr::replace(const string& path) const
{
  for (size_t i = 0; i < m_pathReplacement.size(); i++) {
    const StringPair& x = m_pathReplacement[i];
    const string& x_old = x.first;
    const string& x_new = x.second;

    // Alternative: match substring instead of prefix
    // size_t pos = path.find(x_old); if (pos != string::npos) {}

    if (path.compare(0, x_old.size(), x_old) == 0) {
      string newPath = path;
      newPath.replace(0, x_old.size(), x_new);
      return newPath;
    }
  }
  return path;
}


// Comparison function we use to sort 'pathReplacment' according to the first 
// values of the pair, which is the old partial path.
//
// @param a: The first pair object to compare.
// @param b: The second pair object to compare.
// return:   A bool indicatin if a.first.size() < b.first.size().
static bool
compare_as_strings(const PathReplacementMgr::StringPair& a,
		   const PathReplacementMgr::StringPair& b)
{
  return a.first.size() > b.first.size();
}


void
PathReplacementMgr::addPath(const string& oldPath, const string& newPath)
{
  m_pathReplacement.push_back(StringPair(oldPath, newPath));
  std::stable_sort(m_pathReplacement.begin(), m_pathReplacement.end(),
		   compare_as_strings);
}

