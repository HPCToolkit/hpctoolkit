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

#include "Epoch.hpp"


//*************************** Forward Declarations ***************************

//****************************************************************************

namespace Prof {


Epoch::Epoch(uint sz)
{
  m_lm_byId.reserve(sz);
}


Epoch::~Epoch()
{
  for (uint i = 0; i < lm_size(); ++i) {
    delete lm(i); // Epoch::LM*
  }
  m_lm_byId.clear();
  m_lm_byVMA.clear();
}


Epoch::LM* 
Epoch::lm_find(VMA ip) const
{
  static Epoch::LM key;

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


void Epoch::dump(std::ostream& os) const
{
  std::string pre = "  ";

  os << "{ Prof::Epoch\n";
  for (uint i = 0; i < lm_size(); ++i) {
    os << pre << i << " : ";
    lm(i)->dump(os);
    os << std::endl;
  }

  os << "-----------------------------\n";

  for (LMSet::const_iterator it = lm_begin(); it != lm_end(); ++it) {
    LM* lm = *it;
    os << pre;
    lm->dump(os);
    os << std::endl;
  }
  os << "}\n";
}


void Epoch::ddump() const
{
  dump(std::cerr);
}


//****************************************************************************

Epoch::LM::LM(const char* nm, VMA loadAddr)
  : m_id(LM_id_NULL), m_name((nm) ? nm: ""), 
    m_loadAddr(loadAddr), m_relocAmt(0),
    m_loadAddrPref(0), m_isUsed(false)
{
}


Epoch::LM::~LM()
{
}


void 
Epoch::LM::dump(std::ostream& os) const
{ 
  using namespace std;
  os << "0x" << hex << m_loadAddr << dec << " (+" << m_relocAmt <<  "): " 
     << m_name;
}


void 
Epoch::LM::ddump() const
{
  dump(std::cerr);
}


} // namespace Prof
