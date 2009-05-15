// -*-Mode: C++;-*-
// $Id$

// * BeginRiceCopyright *****************************************************
// 
// Copyright ((c)) 2002-2007, Rice University 
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
//   $Source$
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

namespace Prof {


LoadMap::LoadMap(uint sz)
{
  m_lm_byId.reserve(sz);
}


LoadMap::~LoadMap()
{
  for (uint i = 0; i < lm_size(); ++i) {
    delete lm(i); // LoadMap::LM*
  }
  m_lm_byId.clear();
  m_lm_byName.clear();
  m_lm_byVMA.clear();
}


void 
LoadMap::lm_insert(LoadMap::LM* x) 
{ 
  // FIXME: combine load modules if adjacent

  int id = m_lm_byId.size();
  x->id(id);
  m_lm_byId.push_back(x);
  
  m_lm_byName.insert(x); // multiset insert always successful 

  std::pair<LMSet::iterator, bool> ret = m_lm_byVMA.insert(x);
  if (!ret.second) {
    const LoadMap::LM* y = *(ret.first);
    if (x->name() != y->name()) {
      // Possibly perform additional checking to see y can be folded into x
      DIAG_WMsg(1, "New LoadMap::LM '" << x->toString()
		<< "' conflicts with existing '" << y->toString() << "'");
    }
  }
}


std::pair<LoadMap::LMSet_nm::iterator, LoadMap::LMSet_nm::iterator>
LoadMap::lm_find(const std::string& nm) const
{
  static LoadMap::LM key;
  key.name(nm);

  std::pair<LMSet_nm::iterator, LMSet_nm::iterator> fnd = 
    m_lm_byName.equal_range(&key);
  return fnd;
}


LoadMap::LM* 
LoadMap::lm_find(VMA ip) const
{
  static LoadMap::LM key;

  if (m_lm_byVMA.empty()) {
    return NULL;
  }

  // NOTE: lower_bound(y) returns first z : ((z > y) || (z = y))
  // NOTE: upper_bound(y) returns first z : (z > y)
  key.loadAddr(ip);
  LMSet::iterator it = m_lm_byVMA.upper_bound(&key);
  if (it != m_lm_byVMA.begin()) {
    --it;
    return *it;
  }
  else {
    return *it;
  }
}


void 
LoadMap::compute_relocAmt()
{
  std::string errors;

  for (uint i = 0; i < lm_size(); ++i) {
    try {
      lm(i)->compute_relocAmt();
    }
    catch (const Diagnostics::Exception& x) {
      errors += "  " + x.what() + "\n";
    }
  }
  
  if (!errors.empty()) {
    DIAG_Throw(errors);
  }
}


std::vector<LoadMap::MergeChange> 
LoadMap::merge(const LoadMap& y)
{
  std::vector<MergeChange> mergeChg;
  
  LoadMap& x = *this;

  for (uint i = 0; i < y.lm_size(); ++i) { 
    LoadMap::LM* y_lm = y.lm(i);
    
    std::pair<LoadMap::LMSet_nm::iterator, LoadMap::LMSet_nm::iterator> x_fnd = 
      x.lm_find(y_lm->name());

    LoadMap::LM* x_lm = (x_fnd.first != x_fnd.second) ? *(x_fnd.first) : NULL;
    bool is_x_lm_uniq = (x_lm && (x_fnd.first == --(x_fnd.second)));

    if (is_x_lm_uniq) { // y_lm matches exactly one x_lm
      if (x_lm->id() == y_lm->id()) {
	; // perfect match; nothing to do (common case)
      }
      else { // (x_lm->id() != y_lm->id()) 
	// y_lm->id() is replaced by x_lm->id()
	mergeChg.push_back(MergeChange(y_lm->id(), x_lm->id()));
      }
    }
    else {
      // y_lm matches zero or greater than one x_lm

      // Create x_lm for y_lm.  y_lm->id() is replaced by x_lm->id().
      x_lm = new LoadMap::LM(y_lm->name().c_str(), y_lm->loadAddr(), 
			   y_lm->size());
      lm_insert(x_lm);
      mergeChg.push_back(MergeChange(y_lm->id(), x_lm->id()));
    }

    DIAG_Assert(x_lm->isAvail() == y_lm->isAvail(), "LoadMap::merge: two LoadMap::LM of the same name must both be (un)available: " << x_lm->name());

    x_lm->isUsedMrg(y_lm->isUsed());
  }
  
  return mergeChg;
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
  for (uint i = 0; i < lm_size(); ++i) {
    os << pre << i << " : ";
    lm(i)->dump(os);
    os << std::endl;
  }

  os << "-----------------------------\n";

  for (LMSet::const_iterator it = lm_begin(); it != lm_end(); ++it) {
    LM* lm = *it;
    os << pre << lm->toString() << std::endl;
  }
  os << "}\n";
}


void 
LoadMap::ddump() const
{
  dump(std::cerr);
}


//****************************************************************************

LoadMap::LM::LM(const std::string& name, VMA loadAddr, size_t size)
  : m_id(LM_id_NULL), m_name(name), 
    m_loadAddr(loadAddr), m_size(size), 
    m_isAvail(true),
    m_relocAmt(0),
    m_isUsed(false)
{
}


LoadMap::LM::~LM()
{
}


void 
LoadMap::LM::compute_relocAmt()
{
  BinUtil::LM* lm = new BinUtil::LM();
  try {
    lm->open(m_name.c_str());
    if (lm->doUnrelocate(m_loadAddr)) {
      m_relocAmt = m_loadAddr;
    }
    delete lm;
  }
  catch (...) {
    delete lm;
    m_isAvail = false;
    DIAG_Throw("Cannot open '" << m_name << "'");
  }
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
  using namespace std;
  os << "0x" << hex << m_loadAddr << dec 
     << " (+" << m_relocAmt << " = " 
     << hex << (m_loadAddr + m_relocAmt) << dec << "): " 
     << m_name << " [" << m_id << "]";
}


void 
LoadMap::LM::ddump() const
{
  dump(std::cerr);
}


} // namespace Prof
