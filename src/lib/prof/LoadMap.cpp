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

//************************* System Include Files ****************************

//*************************** User Include Files ****************************

#include "LoadMap.hpp"

#include <lib/binutils/LM.hpp>

#include <lib/support/diagnostics.h>


//*************************** Forward Declarations ***************************

//****************************************************************************

//****************************************************************************
// ALoadMap
//****************************************************************************


namespace Prof {


ALoadMap::ALoadMap(uint sz)
{
  m_lm_byId.reserve(sz);
}


ALoadMap::~ALoadMap()
{
  for (LM_id_t i = 1; i <= size(); ++i) {
    ALoadMap::LM* lm = this->lm(i);
    delete lm;
  }
  m_lm_byId.clear();
}


std::string
ALoadMap::toString() const
{
  std::ostringstream os;
  dump(os);
  return os.str();
}


void 
ALoadMap::ddump() const
{
  dump(std::cerr);
}


//****************************************************************************

ALoadMap::LM::LM(const std::string& name)
  : m_id(LM_id_NULL), m_name(name), m_isUsed(false)
{
}


ALoadMap::LM::~LM()
{
}


std::string
ALoadMap::LM::toString() const
{
  std::ostringstream os;
  dump(os);
  return os.str();
}


void 
ALoadMap::LM::dump(std::ostream& os) const
{ 
  os << m_id << ": " << m_name;
}

 
void 
ALoadMap::LM::ddump() const
{
  dump(std::cerr);
}


} // namespace Prof


//****************************************************************************
// LoadMap
//****************************************************************************

namespace Prof {


LoadMap::LoadMap(uint sz)
  : ALoadMap(sz)
{
}


LoadMap::~LoadMap()
{
  m_lm_byName.clear();
}


void 
LoadMap::lm_insert(ALoadMap::LM* x) 
{ 
  // FIXME: combine load modules if adjacent

  m_lm_byId.push_back(x);
  x->id(m_lm_byId.size()); // 1-based id
  
  m_lm_byName.insert(x); // multiset insert always successful
  // std::pair<LMSet::iterator, bool> ret // FIXME: ensure no duplicate
}


std::pair<LoadMap::LMSet_nm::iterator, LoadMap::LMSet_nm::iterator>
LoadMap::lm_find(const std::string& nm) const
{
  static ALoadMap::LM key;
  key.name(nm);

  std::pair<LMSet_nm::iterator, LMSet_nm::iterator> fnd = 
    m_lm_byName.equal_range(&key);
  return fnd;
}


std::vector<LoadMap::MergeEffect>*
LoadMap::merge(const LoadMap& y)
{
  std::vector<MergeEffect>* mrgEffect = new std::vector<MergeEffect>;
  
  LoadMap& x = *this;

  for (LM_id_t i = 1; i <= y.size(); ++i) { 
    LoadMap::LM* y_lm = y.lm(i);
    
    std::pair<LMSet_nm::iterator, LMSet_nm::iterator> x_fnd = 
      x.lm_find(y_lm->name());

    LoadMap::LM* x_lm = (x_fnd.first != x_fnd.second) ? *(x_fnd.first) : NULL;
    bool is_x_lm_uniq = (x_lm && (x_fnd.first == --(x_fnd.second)));

    if (is_x_lm_uniq) { // y_lm matches exactly one x_lm
      if (x_lm->id() == y_lm->id()) {
	; // perfect match; nothing to do (common case)
      }
      else { // (x_lm->id() != y_lm->id()) 
	// y_lm->id() is replaced by x_lm->id()
	mrgEffect->push_back(MergeEffect(y_lm->id(), x_lm->id()));
      }
    }
    else {
      // y_lm matches zero or greater than one x_lm

      // Create x_lm for y_lm.  y_lm->id() is replaced by x_lm->id().
      x_lm = new LoadMap::LM(y_lm->name());
      lm_insert(x_lm);
      mrgEffect->push_back(MergeEffect(y_lm->id(), x_lm->id()));
    }

    x_lm->isUsedMrg(y_lm->isUsed());
  }
  
  return mrgEffect;
}


void 
LoadMap::dump(std::ostream& os) const
{
  std::string pre = "  ";

  os << "{ Prof::LoadMap\n";
  for (LM_id_t i = 1; i <= size(); ++i) {
    LM* lm = this->lm(i);
    os << pre << i << " : " << lm->toString() << std::endl;
  }
  os << "}\n";
}


} // namespace Prof
