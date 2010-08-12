// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://outreach.scidac.gov/svn/hpctoolkit/trunk/src/lib/support/PathReplacementMgr.hpp $
// $Id: PathReplacementMgr.hpp 2941 2010-07-15 19:18:34Z gvs1 $
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
//   $HeadURL: https://outreach.scidac.gov/svn/hpctoolkit/trunk/src/lib/support/PathReplacementMgr.hpp $
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


  //Searches through the list of paths to see if any portion of
  //'original' needs to be replaced. If a substring of 'original' is
  //found to be in 'pathReplacement', we update that portion of the
  //string with the associated new path.
  //
  //@param original: The file path we want to update
  //@return: A new path that is the updated equivalent of 'original'
  //         if any portion of it needed updating, otherwise we return
  //         'original'.
  std::string
  getReplacedPath(const std::string& original);

  
  //Adds an old path and its associated new path as a pair to the
  //pathReplacement vector and then sorts the list in descending
  //order of size
  // 
  //@param originalPath: The original partial path
  //@param newPath: The new partial path to replace originalPath.
  //
  // **note** - "partial path" can mean any substring of a path, from
  //   a single file name to a full path.
  void
  addPath(const std::string& originalPath, const std::string& newPath);

public:
  typedef std::pair<std:: string, std::string> StringPair;

private:
  std::vector<StringPair> m_pathReplacement;
};


//***************************************************************************

#endif
