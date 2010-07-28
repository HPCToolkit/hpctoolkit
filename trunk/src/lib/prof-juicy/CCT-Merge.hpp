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

#ifndef prof_juicy_Prof_CCT_Merge_hpp 
#define prof_juicy_Prof_CCT_Merge_hpp

//************************* System Include Files ****************************

#include <iostream>

#include <string>
#include <vector>
#include <list>
#include <set>

//*************************** User Include Files ****************************

#include <include/uint.h>

#include <lib/prof-lean/hpcrun-fmt.h>

#include <lib/support/diagnostics.h>


//*************************** Forward Declarations ***************************

//***************************************************************************
// MergeContext
//***************************************************************************

namespace Prof {

namespace CCT {

class Tree;

enum {
  // CCT Merge flags
  MrgFlg_NormalizeTraceFileY = (1 << 0),
  MrgFlg_CCTMergeOnly        = (1 << 1), // Only perform merges (skip adds)
  MrgFlg_AssertCCTMergeOnly  = (1 << 2), // Assert: only perform merges
  
  // *Private* CCT Merge flags
  MrgFlg_PropagateEffects    = (1 << 3),
};


class MergeContext {
public:
  typedef std::set<uint> CPIdSet;

public:
  MergeContext(Tree* cct, bool doTrackCPIds);

  // -------------------------------------------------------
  // 
  // -------------------------------------------------------
  void
  flags(uint mrgFlag)
  { m_mrgFlag = mrgFlag; }

  uint
  flags() const
  { return m_mrgFlag; }

  bool
  doPropagateEffects()
  { return (m_mrgFlag & MrgFlg_PropagateEffects); }


  // -------------------------------------------------------
  //
  // -------------------------------------------------------
  bool
  isTrackingCPIds() const
  { return m_isTrackingCPIds; }


  bool
  isConflict_cpId(uint cpId) const
  {
    return (cpId != HPCRUN_FMT_CCTNodeId_NULL
	    && m_cpIdSet.find(cpId) != m_cpIdSet.end());
  }


  void
  noteCPId(uint cpId)
  {
    std::pair<CPIdSet::iterator, bool> ret = m_cpIdSet.insert(cpId);
    DIAG_Assert(ret.second, "MergeContext::noteCPId: conflicting cp-ids!");
  }


  uint
  makeCPId()
  {
    uint newId = *(m_cpIdSet.rbegin()) + 2;
    std::pair<CPIdSet::iterator, bool> ret = m_cpIdSet.insert(newId);
    DIAG_Assert(hpcrun_fmt_doRetainId(newId) && ret.second,
		"MergeContext::makeCPId: error!");
    return newId;
  }

private:
  void
  fillCPIdSet(Tree* cct);

private:
  const Tree* m_cct;

  uint m_mrgFlag;

  bool m_isTrackingCPIds;
  CPIdSet m_cpIdSet;
};

} // namespace CCT

} // namespace Prof


//***************************************************************************
// MergeEffect, MergeEffectList
//***************************************************************************

namespace Prof {

namespace CCT {

struct MergeEffect {
  MergeEffect()
    : old_cpId(HPCRUN_FMT_CCTNodeId_NULL), new_cpId(HPCRUN_FMT_CCTNodeId_NULL)
  { }
  
  MergeEffect(uint old_, uint new_) : old_cpId(old_), new_cpId(new_)
  { }
  
  bool
  isNoop()
  {
    return (old_cpId == HPCRUN_FMT_CCTNodeId_NULL &&
	      new_cpId == HPCRUN_FMT_CCTNodeId_NULL);
  }

  std::string
  toString(const char* pfx = "") const;

  std::ostream&
  dump(std::ostream& os = std::cerr, const char* pfx = "") const;


  static std::string
  toString(const std::list<MergeEffect>& effctLst,
	   const char* pfx = "");

  static std::ostream&
  dump(const std::list<CCT::MergeEffect>& effctLst,
       std::ostream& os, const char* pfx = "");

  uint old_cpId /*within y*/, new_cpId /*within y*/;
};


typedef std::list<MergeEffect> MergeEffectList;


} // namespace CCT

} // namespace Prof


//***************************************************************************

#endif /* prof_juicy_Prof_CCT_Merge_hpp */
