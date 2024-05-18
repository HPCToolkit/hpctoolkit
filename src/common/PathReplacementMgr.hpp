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

#ifndef PathReplacementMgr_hpp
#define PathReplacementMgr_hpp

//************************* System Include Files ****************************

#include <string>
#include <vector>

//*************************** User Include Files ****************************

//*************************** Forward Declarations **************************

//***************************************************************************
// PathReplacementMgr
//***************************************************************************

class PathReplacementMgr {

public:
  PathReplacementMgr();
  ~PathReplacementMgr();


  static PathReplacementMgr&
  singleton();


  // replace(): Given 'path', search through the list of
  //   replacement-path pairs looking for a match.  If a prefix of
  //   'path' matches an 'old-path', replace that matched substring
  //   with the corresponding 'new-path'.  Stops after first match.
  //
  // @param path: The path upon which to perform replacement
  // @return: The updated path.
  std::string
  replace(const std::string& path) const;


  // Adds an old path and its associated new path as a pair to the
  // pathReplacement vector and then sorts the list in descending
  // order of size
  //
  // @param oldPath: The original partial path, which can be any
  //   substring of a path.
  // @param newPath: The new partial path to replace oldPath.
  void
  addPath(const std::string& oldPath, const std::string& newPath);

public:
  typedef std::pair<std:: string, std::string> StringPair;

private:
  std::vector<StringPair> m_pathReplacement;
};


//***************************************************************************

#endif
