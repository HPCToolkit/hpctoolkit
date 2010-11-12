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

#ifndef prof_Prof_LoadMap_hpp
#define prof_Prof_LoadMap_hpp

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

#include <lib/prof-lean/hpcrun-fmt.h>

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
  static const LM_id_t LM_id_NULL = HPCRUN_FMT_LMId_NULL;

  // ------------------------------------------------------------
  // 
  // ------------------------------------------------------------
  class LM : public Unique {
  public:
    LM(const std::string& name = "");
    //LM(const char* name = NULL);

    virtual ~LM();

    
    LM_id_t
    id() const
    { return m_id; }
    

    const std::string&
    name() const
    { return m_name; }

    void
    name(std::string x)
    { m_name = x; }

    void
    name(const char* x)
    { m_name = (x) ? x: ""; }


    // isUsed: e.g., does this ALoadMap::LM have associated measurement data
    bool
    isUsed() const
    { return m_isUsed; }

    void
    isUsed(bool x)
    { m_isUsed = x; }

    void
    isUsedMrg(bool x)
    { m_isUsed = (m_isUsed || x); }

    
    std::string
    toString() const;

    void
    dump(std::ostream& os = std::cerr) const;
    
    void
    ddump() const;

  private:
    void
    id(LM_id_t x)
    { m_id = x; }

    friend class LoadMap;
    friend class LoadMapMgr;
    
  private: 
    LM_id_t m_id;
    std::string m_name;
    bool m_isUsed;
  };


  typedef std::vector<LM*> LMVec;


  struct MergeEffect {
    MergeEffect(LM_id_t old_, LM_id_t new_) : old_id(old_), new_id(new_) { }
    LM_id_t old_id /*in y*/, new_id /*in x */;
  };

  
public:
  ALoadMap(const uint i = 32);

  virtual ~ALoadMap();

  // assumes ownership
  virtual void
  lm_insert(ALoadMap::LM* x) = 0;

  
  // ------------------------------------------------------------
  // Access by id (1-based!)
  // ------------------------------------------------------------
  LM_id_t
  size() const
  { return m_lm_byId.size(); }

  // N.B.: LM_id_t's are 1-based since 0 is a NULL value
  LM*
  lm(LM_id_t id) const
  { return m_lm_byId[(id - 1)]; }


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


inline bool
operator<(const Prof::ALoadMap::LM x, const Prof::ALoadMap::LM y)
{
  return (x.id() < y.id());
}

//****************************************************************************
// LoadMap
//****************************************************************************

//***************************************************************************

#endif /* prof_Prof_LoadMap_hpp */
