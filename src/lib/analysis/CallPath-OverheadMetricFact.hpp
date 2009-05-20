// -*-Mode: C++;-*-
// $Id: CallPath.hpp 2101 2009-04-02 16:35:28Z tallent $

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

#ifndef Analysis_CallPath_OverheadMetricFact_hpp 
#define Analysis_CallPath_OverheadMetricFact_hpp

//************************* System Include Files ****************************

#include <iostream>
#include <vector>
#include <stack>
#include <string>

//*************************** User Include Files ****************************

#include <include/uint.h>

#include <lib/prof-juicy/CallPath-Profile.hpp>

//*************************** Forward Declarations ***************************

//****************************************************************************

namespace Analysis {

namespace CallPath {


class OverheadMetricFact
{
public:
  OverheadMetricFact() { }
  
  ~OverheadMetricFact() { }
  
  // make 'overhead' and 'work' metrics
  void 
  make(Prof::CallPath::Profile* prof);
  
  static inline bool 
  isOverhead(const Prof::CCT::ProcFrm* x)
  {
    const string& x_fnm = x->fileName();
    if (x_fnm.length() >= s_tag.length()) {
      size_t tag_beg = x_fnm.length() - s_tag.length();
      return (x_fnm.compare(tag_beg, s_tag.length(), s_tag) == 0);
    }
    return false;
  }

private:

  void 
  make(Prof::CCT::ANode* node, 
       const std::vector<uint>& m_src, const std::vector<uint>& m_dst, 
       bool isOverheadCtxt);

  static inline bool 
  isMetricSrc(const Prof::SampledMetricDesc* mdesc)
  {
    const string& nm = mdesc->name();
    return ((nm.find("PAPI_TOT_CYC") == 0) || (nm.find("WALLCLOCK") == 0));
  }

  static void
  convertToWorkMetric(Prof::SampledMetricDesc* mdesc);

  static const string s_tag;  

};


} // namespace CallPath

} // namespace Analysis

//****************************************************************************

#endif // Analysis_CallPath_OverheadMetricFact_hpp
