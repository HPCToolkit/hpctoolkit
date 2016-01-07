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

#ifndef RealPathMgr_hpp
#define RealPathMgr_hpp

//************************* System Include Files ****************************

#include <string>
#include <map>
#include <iostream>

#include <cctype>

//*************************** User Include Files ****************************

#include <include/uint.h>

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
  toString(uint flags = 0) const;

  // flags = -1: compressed dump / 0: normal dump / 1: extra info
  std::ostream&
  dump(std::ostream& os, uint flags = 0, const char* pfx = "") const;

  void
  ddump(uint flags = 0) const;


private:
  typedef std::map<std::string, std::string> MyMap;

  std::string m_searchPaths;
  mutable MyMap m_cache;
};


//***************************************************************************

#endif // RealPathMgr_hpp
