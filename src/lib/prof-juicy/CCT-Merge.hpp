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
