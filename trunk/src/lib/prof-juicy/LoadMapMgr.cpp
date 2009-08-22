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

//************************* System Include Files ****************************

//*************************** User Include Files ****************************

#include "LoadMapMgr.hpp"

#include <lib/support/diagnostics.h>


//*************************** Forward Declarations ***************************

//****************************************************************************

namespace Prof {


LoadMapMgr::LoadMapMgr(uint sz)
  : ALoadMap(sz)
{
}


LoadMapMgr::~LoadMapMgr()
{
  m_lm_byName.clear();
}


void 
LoadMapMgr::lm_insert(LoadMap::LM* x)
{ 
  int id = m_lm_byId.size();
  x->id(id);
  m_lm_byId.push_back(x);
  
  std::pair<LMSet_nm::iterator, bool> ret = m_lm_byName.insert(x);
  DIAG_Assert(ret.second, "LoadMapMgr::lm_insert(): conflict inserting: " 
	      << x->toString());
}


LoadMapMgr::LMSet_nm::iterator
LoadMapMgr::lm_find(const std::string& nm) const
{
  static LoadMap::LM key;
  key.name(nm);

  LMSet_nm::iterator fnd = m_lm_byName.find(&key);
  return fnd;
}


std::vector<LoadMap::MergeChange> 
LoadMapMgr::merge(const ALoadMap& y)
{
  std::vector<LoadMap::MergeChange> mergeChg;
  
  LoadMapMgr& x = *this;

  for (uint i = 0; i < y.size(); ++i) { 
    LoadMap::LM* y_lm = y.lm(i);
    
    LMSet_nm::iterator x_fnd = x.lm_find(y_lm->name());
    LoadMap::LM* x_lm = (x_fnd != x.lm_end_nm()) ? *x_fnd : NULL;

    if (x_lm) {
      if (x_lm->id() != y_lm->id()) {
	// y_lm->id() is replaced by x_lm->id()
	mergeChg.push_back(LoadMap::MergeChange(y_lm->id(), x_lm->id()));
      }
    }
    else {
      // Create x_lm for y_lm.  y_lm->id() is replaced by x_lm->id().
      x_lm = new ALoadMap::LM(y_lm->name());
      x_lm->isAvail(y_lm->isAvail());
      lm_insert(x_lm);
      mergeChg.push_back(LoadMap::MergeChange(y_lm->id(), x_lm->id()));
    }
    
    DIAG_Assert(x_lm->isAvail() == y_lm->isAvail(), "LoadMapMgr::merge: two LoadMapMgr::LM of the same name must both be (un)available: " << x_lm->name());

    x_lm->isUsedMrg(y_lm->isUsed());
  }
  
  return mergeChg;
}


void 
LoadMapMgr::dump(std::ostream& os) const
{
  std::string pre = "  ";

  os << "{ Prof::LoadMapMgr\n";
  for (uint i = 0; i < size(); ++i) {
    os << pre << i << " : ";
    lm(i)->dump(os);
    os << std::endl;
  }
  os << "}\n";
}


} // namespace Prof
