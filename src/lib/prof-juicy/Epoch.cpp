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

#include <lib/xml/xml.hpp>
using namespace xml;

//*************************** Forward Declarations ***************************

//****************************************************************************

namespace Prof {


Epoch::Epoch(const unsigned int i)
  : m_lmVec(i, NULL)
{
} 


void Epoch::dump(std::ostream& o)
{
  for (int i = 0; i < m_lmVec.size(); ++i) {
    LM* lm = m_lmVec[i];
    lm->dump(o);
  }
}


void Epoch::ddump()
{
  dump(std::cerr);
}


Epoch::~Epoch()
{
  for (LMVec::iterator it = lm_begin(); it != lm_end(); ++it) {
    delete *it; // Epoch::LM*
  }
  m_lmVec.clear();
}


Epoch::LM* 
Epoch::lm_find(VMA ip)
{
  LM* pre = m_lmVec[0];

  for (int i = 1; i < m_lmVec.size(); ++i) {
    LM* curr = m_lmVec[i];
    if (pre->loadAddr() <= ip && ip < curr->loadAddr())
      return pre;
    else 
      pre = curr;
  }
  
  return pre;
}


//****************************************************************************

Epoch::LM::LM()
{
  lm = NULL;
}


Epoch::LM::~LM()
{
  delete lm;
}


void 
Epoch::LM::dump(std::ostream& o)
{ 
  using std::hex;
  using std::dec;
  using std::endl; 
  
  o << m_name;
  o <<" m_loadAddr is 0x" << hex <<  m_loadAddr;
  o <<" m_prefAddr is 0x" << hex  << m_prefAddr;
  o << dec << endl; 
}


void 
Epoch::LM::ddump()
{
  dump(std::cerr);
}


} // namespace Prof
