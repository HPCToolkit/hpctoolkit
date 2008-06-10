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

#ifndef prof_juicy_Prof_Epoch_hpp
#define prof_juicy_Prof_Epoch_hpp

//************************* System Include Files ****************************

#include <string>
#include <vector>
#include <algorithm>
#include <iostream>

//*************************** User Include Files ****************************

#include <include/general.h>

#include <lib/isa/ISATypes.hpp>

#include <lib/binutils/LM.hpp>

#include <lib/support/NonUniformDegreeTree.hpp>
#include <lib/support/Unique.hpp>

//*************************** Forward Declarations ***************************

namespace Prof {

//****************************************************************************
// Epoch
//****************************************************************************

class Epoch : public Unique {
public:

  class LM : public Unique {
  public:
    LM();
    virtual ~LM();
    
    const std::string& name() const
      { return m_name; }
    void name(const char* s) 
      { m_name = (s) ? s: ""; }
    
    VMA loadAddr() const 
      { return m_loadAddr; }
    void loadAddr(VMA addr) 
      { m_loadAddr = addr; }

    // FIXME: why do we want these three sets of functions here?
    VMA GetVaddr() const   { return m_prefAddr; }
    void SetVaddr(VMA  v)  { m_prefAddr = v; }

    bool GetUsedFlag() const { return used; }
    void SetUsedFlag(bool b) { used=b; }

    bool LMIsEmpty()            { return (lm == NULL); }
    void SetLM(binutils::LM* x) { lm = x; }
    binutils::LM* GetLM()       { return lm; }
    
    void dump(std::ostream& o= std::cerr);
    void ddump();
    
  private: 
    std::string m_name;
    VMA m_loadAddr;

    VMA m_prefAddr;
    bool used;
    binutils::LM* lm;
  };

  struct cmp_LM_loadAddr
  {
    bool operator()(const LM* a, const LM* b) const
    { return (a->loadAddr() < b->loadAddr()); }
  };
  
  typedef std::vector<LM*> LMVec;
  
public: 
  Epoch(const unsigned int i);
  virtual ~Epoch();
  

  uint lm_size() { return m_lmVec.size(); } 

  LM* lm(uint i) { return m_lmVec[i]; }
  void lm(uint i, const LM* lm) { m_lmVec[i] = const_cast<LM*>(lm); }

  LM* lm_find(VMA ip);

  //void lm_add(LM* ldm) { m_lmVec.push_back(ldm); }
  

  void SortLoadmoduleByVMA() {
    std::sort(m_lmVec.begin(), m_lmVec.end(), cmp_LM_loadAddr());
  } 


  // iterators:
  LMVec::iterator lm_begin() 
    { return m_lmVec.begin(); }
  LMVec::const_iterator lm_begin() const 
    { return m_lmVec.begin(); }
  LMVec::iterator lm_end() 
    { return m_lmVec.end(); }
  LMVec::const_iterator lm_end() const 
    { return m_lmVec.end(); }
  
  // Debug
  void dump(std::ostream& o = std::cerr);
  void ddump();
  
protected:
private:
  LMVec  m_lmVec;
};


} // namespace Prof

//***************************************************************************

#endif /* prof_juicy_Prof_Epoch_hpp */
