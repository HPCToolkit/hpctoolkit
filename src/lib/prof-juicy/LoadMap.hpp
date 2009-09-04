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

#ifndef prof_juicy_Prof_LoadMap_hpp
#define prof_juicy_Prof_LoadMap_hpp

//************************* System Include Files ****************************

#include <iostream>

#include <string>

#include <vector>
#include <set>

#include <algorithm>


//*************************** User Include Files ****************************

#include <include/uint.h>

#include <lib/isa/ISATypes.hpp>

#include <lib/binutils/LM.hpp>

#include <lib/support/diagnostics.h>
#include <lib/support/Unique.hpp>


//*************************** Forward Declarations ***************************


//****************************************************************************
// ALoadMap
//****************************************************************************

namespace Prof {

class ALoadMap : public Unique {
public:

  // N.B.: Life is much easier if this is consistent with hpcrun-fmt
  typedef uint LM_id_t;
  static const LM_id_t LM_id_NULL = 0;

  class LM : public Unique {
  public:
    LM(const std::string& name = "", VMA loadAddr = 0, size_t size = 0);
    //LM(const char* name = NULL, VMA loadAddr = 0, size_t size = 0);
    virtual ~LM();

    LM_id_t id() const 
      { return m_id; }
    
    const std::string& name() const
      { return m_name; }
    void name(std::string x)
      { m_name = x; }
    void name(const char* x) 
      { m_name = (x) ? x: ""; }

    VMA loadAddr() const 
      { return m_loadAddr; }
    void loadAddr(VMA x) 
      { m_loadAddr = x; }

    VMA size() const 
      { return m_size; }
    void size(size_t x) 
      { m_size = x; }

    // isAvail: this ALoadMap::LM is active in the sense that the
    // associated load module is available and relocation information
    // is accurate.  NOTE: a module is not available, e.g., if system
    // libs changed or the user is executing on a different system.
    bool isAvail() const
      { return m_isAvail; }
    void isAvail(bool x)
      { m_isAvail = x; }

    // relocate_VMA - relocAmt() = unrelocated_VMA
    VMA relocAmt() const
      { return m_relocAmt; }
    void relocAmt(VMA x)
      { m_relocAmt = x; }
    
    void compute_relocAmt();


    // isUsed: does this ALoadMap::LM have data in the CCT
    // tallent: FIXME: should not be located here
    bool isUsed() const { return m_isUsed; }
    void isUsed(bool x) { m_isUsed = x; }
    void isUsedMrg(bool x) { m_isUsed = (m_isUsed || x); }

    
    std::string toString() const;

    void dump(std::ostream& os = std::cerr) const;
    void ddump() const;

  private:
    void id(LM_id_t x)
      { m_id = x; }

    friend class LoadMap;
    friend class LoadMapMgr;
    
  private: 
    LM_id_t m_id;
    std::string m_name;
    VMA m_loadAddr;
    size_t m_size;
    size_t m_isAvail;

    VMA m_relocAmt;
    
    bool m_isUsed;
  };


  typedef std::vector<LM*> LMVec;


  struct MergeChange {
    MergeChange(LM_id_t old_, LM_id_t new_) : old_id(old_), new_id(new_) { }
    LM_id_t old_id /*in y*/, new_id /*in x */;
  };

  
public: 
  ALoadMap(const uint i = 16);
  virtual ~ALoadMap();

  // assumes ownership
  virtual void 
  lm_insert(ALoadMap::LM* x) = 0;

  
  // ------------------------------------------------------------
  // Access by id (1-based!)
  // ------------------------------------------------------------
  uint size() const
  { return m_lm_byId.size(); } 

  // N.B.: 1-based since 0 is a NULL value
  LM* lm(LM_id_t i) const
  { return m_lm_byId[(i - 1)]; }


  // ------------------------------------------------------------
  // 
  // ------------------------------------------------------------

  std::string 
  toString() const;

  virtual void 
  dump(std::ostream& os = std::cerr) const = 0;

  void 
  ddump() const;

protected:
  LMVec m_lm_byId;
};

} // namespace Prof


//****************************************************************************
// LoadMap
//****************************************************************************

namespace Prof {

class LoadMap : public ALoadMap {
public:

  struct lt_LM_nm
  {
    inline bool 
    operator()(const ALoadMap::LM* x, const ALoadMap::LM* y) const
    { 
      return (x->name() < y->name()); 
    }
  };

  struct lt_LM_vma
  {
    inline bool 
    operator()(const ALoadMap::LM* x, const ALoadMap::LM* y) const
    { 
      return (x->loadAddr() < y->loadAddr()); 
    }
  };

  typedef std::multiset<ALoadMap::LM*, lt_LM_nm> LMSet_nm;
  typedef std::set<ALoadMap::LM*, lt_LM_vma> LMSet;
  
public: 
  LoadMap(const uint i = 16);
  virtual ~LoadMap();

  // assumes ownership
  virtual void 
  lm_insert(ALoadMap::LM* x);
  
  // ------------------------------------------------------------
  // Access by id: from ALoadMap
  // ------------------------------------------------------------

  // ------------------------------------------------------------
  // Access by name
  // 
  // NOTE: this is a multiset since different portions of the same
  // load module may be loaded at different places
  // ------------------------------------------------------------
  std::pair<LMSet_nm::iterator, LMSet_nm::iterator>
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
  // Access by VMA
  // ------------------------------------------------------------
  ALoadMap::LM* lm_find(VMA ip) const;

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
  void compute_relocAmt();


  // merge: Given an LoadMap y, merge y into x = 'this'.  Returns a
  // vector of MergeChange describing changes that were made.  The
  // vector contains at most one MergeChange for each LM_id_t (old_id)
  // in y.
  //
  // NOTE: x and y must not conflict! (See lm_insert().)
  std::vector<MergeChange> 
  merge(const LoadMap& y);


  // ------------------------------------------------------------
  // 
  // ------------------------------------------------------------

  virtual void 
  dump(std::ostream& os = std::cerr) const;

private:
  LMSet_nm m_lm_byName;
  LMSet    m_lm_byVMA;
};

} // namespace Prof

inline bool 
operator<(const Prof::LoadMap::LM x, const Prof::LoadMap::LM y)
{
  return (x.loadAddr() < y.loadAddr());
}

//***************************************************************************

#endif /* prof_juicy_Prof_LoadMap_hpp */
