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
// Copyright ((c)) 2002-2016, Rice University
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

#include <iostream>

#include <string>
using std::string;


//*************************** User Include Files ****************************

#include <include/gcc-attr.h>
#include <include/uint.h>

#include "CCT-Merge.hpp"

#include "CCT-Tree.hpp"

#include <lib/support/diagnostics.h>


//*************************** Forward Declarations ***************************

//***************************************************************************


namespace Prof {

namespace CCT {

//***************************************************************************
// MergeContext
//***************************************************************************

MergeContext::MergeContext(Tree* cct, bool doTrackCPIds)
  : m_cct(cct), m_mrgFlag(0), m_isTrackingCPIds(doTrackCPIds)
{
  if (isTrackingCPIds()) {
    fillCPIdSet(cct);
  }
}


void
MergeContext::fillCPIdSet(Tree* cct)
{
  for (ANodeIterator it(cct->root()); it.Current(); ++it) {
    ANode* n = it.current();
    ADynNode* n_dyn = dynamic_cast<ADynNode*>(n);

    if (n_dyn && n_dyn->cpId() != 0) {
      m_cpIdSet.insert(n_dyn->cpId());
    }
  }
}


//***************************************************************************
// MergeEffect
//***************************************************************************

string
MergeEffect::toString(const char* pfx) const
{
  std::ostringstream os;
  dump(os, pfx);
  return os.str();
}


std::ostream&
MergeEffect::dump(std::ostream& os, const char* GCC_ATTR_UNUSED pfx) const
{
  os << old_cpId << " => " << new_cpId;
  return os;
}


string
MergeEffect::toString(const MergeEffectList& effctLst,
			     const char* pfx)
{
  std::ostringstream os;
  dump(effctLst, os, pfx);
  return os.str();
}


std::ostream&
MergeEffect::dump(const MergeEffectList& effctLst,
			 std::ostream& os, const char* pfx)
{
  os << "{ ";
  for (MergeEffectList::const_iterator it = effctLst.begin();
       it != effctLst.end(); ++it) {
    const MergeEffect& effct = *it;
    os << "[ ";
    effct.dump(os, pfx);
    os << "] ";
  }
  os << "}";
  return os;
}


} // namespace CCT

} // namespace Prof

