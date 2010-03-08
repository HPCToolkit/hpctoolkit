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

  static RealPathMgr& singleton();

  // -------------------------------------------------------
  // 
  // -------------------------------------------------------

  // realpath: Given 'fnm', convert it to its 'realpath' (if possible)
  // and return true.  Return true if 'fnm' is as fully resolved as it
  // can be (which does not necessarily mean it exists); otherwise
  // return false.
  bool realpath(std::string& fnm);
  
  const std::string& searchPaths()
    { return m_searchPaths; }

  void searchPaths(const std::string& x)
    { m_searchPaths = x; }

  // -------------------------------------------------------
  // debugging
  // -------------------------------------------------------
  std::string toString(int flags = 0) const;

  // flags = -1: compressed dump / 0: normal dump / 1: extra info
  std::ostream& dump(std::ostream& os, int flags = 0) const;

  void ddump(int flags = 0) const;

private:
  typedef std::map<std::string, std::string> MyMap;
  
  std::string m_searchPaths;
  MyMap m_realpath_map;
};



//***************************************************************************

#endif // RealPathMgr_hpp
