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

//*************************** User Include Files ****************************

#include "LoadMap.hpp"

#include "Struct-Tree.hpp" // TODO: Prof::Struct::Tree::UnknownLMNm

#include <lib/binutils/LM.hpp>

#include <lib/support/diagnostics.h>


//*************************** Forward Declarations ***************************

//****************************************************************************


//****************************************************************************
// LoadMap
//****************************************************************************

namespace Prof {


LoadMap::LoadMap(uint sz)
{
  m_lm_byId.reserve(sz);

  LM* nullLM = new LM(Prof::Struct::Tree::UnknownLMNm);
  lm_insert(nullLM);
  DIAG_Assert(nullLM->id() == LoadMap::LMId_NULL, "LoadMap::LoadMap");
}


LoadMap::~LoadMap()
{
  for (LMId_t i = LoadMap::LMId_NULL; i <= size(); ++i) {
    LoadMap::LM* lm = this->lm(i);
    delete lm;
  }
  m_lm_byId.clear();
  m_lm_byName.clear();
}


void
LoadMap::lm_insert(LoadMap::LM* x)
{
  m_lm_byId.push_back(x);
  x->id(m_lm_byId.size() - 1); // id is the last slot used
 
  std::pair<LMSet_nm::iterator, bool> ret = m_lm_byName.insert(x);
  DIAG_Assert(ret.second, "LoadMap::lm_insert(): conflict inserting: "
	      << x->toString());
}


LoadMap::LMSet_nm::iterator
LoadMap::lm_find(const std::string& nm) const
{
  static LoadMap::LM key;
  key.name(nm);

  LMSet_nm::iterator fnd = m_lm_byName.find(&key);
  return fnd;
}


std::vector<LoadMap::MergeEffect>*
LoadMap::merge(const LoadMap& y)
{
  std::vector<LoadMap::MergeEffect>* mrgEffect =
    new std::vector<LoadMap::MergeEffect>;
  
  LoadMap& x = *this;

  for (LMId_t i = LoadMap::LMId_NULL; i <= y.size(); ++i) {
    LoadMap::LM* y_lm = y.lm(i);
    
    LMSet_nm::iterator x_fnd = x.lm_find(y_lm->name());
    LoadMap::LM* x_lm = (x_fnd != x.lm_end_nm()) ? *x_fnd : NULL;

    // Post-INVARIANT: A corresponding x_lm exists
    if (!x_lm) {
      // Create x_lm for y_lm.
      x_lm = new LoadMap::LM(y_lm->name());
      lm_insert(x_lm);
    }

    if (x_lm->id() != y_lm->id()) {
      // y_lm->id() is replaced by x_lm->id()
      mrgEffect->push_back(LoadMap::MergeEffect(y_lm->id(), x_lm->id()));
    }
    
    x_lm->isUsedMrg(y_lm->isUsed());
  }
  
  return mrgEffect;
}


std::string
LoadMap::toString() const
{
  std::ostringstream os;
  dump(os);
  return os.str();
}


void
LoadMap::dump(std::ostream& os) const
{
  std::string pre = "  ";

  os << "{ Prof::LoadMap\n";
  for (LMId_t i = LoadMap::LMId_NULL; i <= size(); ++i) {
    LoadMap::LM* lm = this->lm(i);
    os << pre << i << " : " << lm->toString() << std::endl;
  }
  os << "}\n";
}


void
LoadMap::ddump() const
{
  dump(std::cerr);
}


//****************************************************************************
// LoadMap::LM
//****************************************************************************

LoadMap::LM::LM(const std::string& name)
  : m_id(LMId_NULL), m_name(name), m_isUsed(false)
{
}


LoadMap::LM::~LM()
{
}


std::string
LoadMap::LM::toString() const
{
  std::ostringstream os;
  dump(os);
  return os.str();
}


void
LoadMap::LM::dump(std::ostream& os) const
{
  os << m_id << ": " << m_name;
}


void
LoadMap::LM::ddump() const
{
  dump(std::cerr);
}


} // namespace Prof
