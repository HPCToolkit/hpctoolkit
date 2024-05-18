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
