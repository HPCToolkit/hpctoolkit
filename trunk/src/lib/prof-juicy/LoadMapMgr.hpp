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
// Copyright ((c)) 2002-2009, Rice University 
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

#ifndef prof_juicy_Prof_LoadMapMgr_hpp
#define prof_juicy_Prof_LoadMapMgr_hpp

//************************* System Include Files ****************************

#include <iostream>

#include <string>

#include <set>

//*************************** User Include Files ****************************

#include <include/uint.h>

#include "LoadMap.hpp"

#include <lib/support/diagnostics.h>
#include <lib/support/Unique.hpp>


//*************************** Forward Declarations ***************************

namespace Prof {

//****************************************************************************
// LoadMapMgr
//****************************************************************************

class LoadMapMgr : public ALoadMap {
public:
  typedef std::set<ALoadMap::LM*, LoadMap::lt_LM_nm> LMSet_nm;

public: 
  LoadMapMgr(const uint i = 16);
  virtual ~LoadMapMgr();

  // assumes ownership
  virtual void 
  lm_insert(ALoadMap::LM* x);

  // ------------------------------------------------------------
  // Access by id: ALoadMap
  // ------------------------------------------------------------

  // ------------------------------------------------------------
  // Access by name
  // ------------------------------------------------------------
  LMSet_nm::iterator
  lm_find(const std::string& nm) const;

  LMSet_nm::iterator lm_begin_nm() 
  { return m_lm_byName.begin(); }

  LMSet_nm::const_iterator lm_begin_nm() const 
  { return m_lm_byName.begin(); }

  LMSet_nm::iterator lm_end_nm() 
  { return m_lm_byName.end(); }

  LMSet_nm::const_iterator lm_end_nm() const 
  { return m_lm_byName.end(); }


  // ------------------------------------------------------------
  // 
  // ------------------------------------------------------------

  // merge: Given a LoadMap y, merge y into x = 'this'.  Returns a
  // vector of MergeChange describing changes that were made.  The
  // vector contains at most one MergeChange for each LM_id_t (old_id)
  // in y.
  std::vector<ALoadMap::MergeChange> 
  merge(const ALoadMap& y);

  // ------------------------------------------------------------
  // 
  // ------------------------------------------------------------

  virtual void 
  dump(std::ostream& os = std::cerr) const;

private:
  LMSet_nm m_lm_byName;
};

} // namespace Prof


//***************************************************************************

#endif /* prof_juicy_Prof_LoadMapMgr_hpp */
