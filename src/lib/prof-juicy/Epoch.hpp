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

#include <iostream>

#include <string>

#include <vector>
#include <set>

#include <algorithm>


//*************************** User Include Files ****************************

#include <include/general.h>

#include <lib/isa/ISATypes.hpp>

#include <lib/binutils/LM.hpp>

#include <lib/support/diagnostics.h>
#include <lib/support/Unique.hpp>


//*************************** Forward Declarations ***************************

namespace Prof {

//****************************************************************************
// Epoch
//****************************************************************************

class Epoch : public Unique {
public:

  typedef int LM_id_t;
  static const LM_id_t LM_id_NULL = -1;

  class LM : public Unique {
  public:
    LM(const char* name = NULL, VMA loadAddr = 0);
    virtual ~LM();

    LM_id_t id() const 
      { return m_id; }
    
    const std::string& name() const
      { return m_name; }
    void name(const char* x) 
      { m_name = (x) ? x: ""; }
    
    VMA loadAddr() const 
      { return m_loadAddr; }
    void loadAddr(VMA addr) 
      { m_loadAddr = addr; }

#if 0
    // tallent: I don't think we want this here
    VMA loadAddrPref() const  { return m_loadAddrPref; }
    void loadAddrPref(VMA x)  { m_loadAddrPref = x; }
#endif

    // tallent: FIXME: should not be located here
    bool isUsed() const { return m_isUsed; }
    void isUsed(bool x) { m_isUsed = x; }
    
    void dump(std::ostream& os = std::cerr) const;
    void ddump() const;

  private:
    void id(uint x)
      { m_id = x; }

    friend class Epoch;
    
  private: 
    LM_id_t m_id;
    std::string m_name;
    VMA m_loadAddr;
    VMA m_loadAddrPref;
    bool m_isUsed;
  };

  struct lt_LM
  {
    inline bool 
    operator()(const LM* x, const LM* y) const
    { 
      return (x->loadAddr() < y->loadAddr()); 
    }
  };
  
  typedef std::vector<LM*> LMVec;
  typedef std::set<LM*, lt_LM> LMSet;
  
public: 
  Epoch(const uint i = 16);
  virtual ~Epoch();

  // assumes ownership
  void lm_insert(Epoch::LM* x) 
  { 
    int id = m_lm_byId.size();
    x->id(id);
    m_lm_byId.push_back(x);

    m_lm_byVMA.insert(x);
  }
  
  // ------------------------------------------------------------
  // Access by id
  // ------------------------------------------------------------
  uint lm_size() const
  { return m_lm_byId.size(); } 

  LM* lm(uint i) const
  { return m_lm_byId[i]; }

  // ------------------------------------------------------------
  // Access by VMA
  // ------------------------------------------------------------
  LM* lm_find(VMA ip) const;

  LMSet::iterator lm_begin() 
  { return m_lm_byVMA.begin(); }

  LMSet::const_iterator lm_begin() const 
  { return m_lm_byVMA.begin(); }

  LMSet::iterator lm_end() 
  { return m_lm_byVMA.end(); }

  LMSet::const_iterator lm_end() const 
  { return m_lm_byVMA.end(); }


  LMSet::reverse_iterator lm_rbegin()
  { return m_lm_byVMA.rbegin(); }

  LMSet::const_reverse_iterator lm_rbegin() const
  { return m_lm_byVMA.rbegin(); }

  LMSet::reverse_iterator lm_rend()
  { return m_lm_byVMA.rend(); }

  LMSet::const_reverse_iterator lm_rend() const
  { return m_lm_byVMA.rend(); }

  // ------------------------------------------------------------
  // 
  // ------------------------------------------------------------
  void dump(std::ostream& os = std::cerr) const;
  void ddump() const;
  
protected:
#if 0
  void lm_sort() {
    std::sort(m_lm_byVMA.begin(), m_lm_byVMA.end(), lt_LM());
  } 
#endif

private:
  LMVec m_lm_byId;
  LMSet m_lm_byVMA;
};

} // namespace Prof

inline bool 
operator<(const Prof::Epoch::LM x, const Prof::Epoch::LM y)
{
  return (x.loadAddr() < y.loadAddr());
}

//***************************************************************************

#endif /* prof_juicy_Prof_Epoch_hpp */
