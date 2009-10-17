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

//************************* System Include Files ****************************

#include <iostream>

#include <string>
using std::string;

#include <typeinfo>


//*************************** User Include Files ****************************

#include "CallPath-OverheadMetricFact.hpp"

#include <lib/support/diagnostics.h>
#include <lib/support/Logic.hpp>

//*************************** Forward Declarations ***************************


//****************************************************************************
// 
//****************************************************************************

namespace Analysis {

namespace CallPath {

//***************************************************************************


// Assumes: metrics are still only at leaves (CCT::Stmt)
void 
OverheadMetricFact::make(Prof::CallPath::Profile& prof)
{
  using namespace Prof;
  
  // ------------------------------------------------------------
  // Create overhead metric descriptors and mapping from source
  //   metrics to overhead metrics
  // ------------------------------------------------------------
  std::vector<uint> metric_src;
  std::vector<uint> metric_dst;
  
  uint numMetrics_orig = prof.metricMgr()->size();
  for (uint m_id = 0; m_id < numMetrics_orig; ++m_id) {
    Metric::ADesc* m_desc = prof.metricMgr()->metric(m_id);
    if (OverheadMetricFact::isMetricSrc(m_desc)) {
      OverheadMetricFact::convertToWorkMetric(m_desc);
      metric_src.push_back(m_id);

      string nm = "overhead";
      const string& nm_sfx = m_desc->nameSfx();
      if (!nm_sfx.empty()) {
	nm += " " + nm_sfx;
      }
      string desc = "parallel overhead";

      Metric::DerivedDesc* m_new =
	new Metric::DerivedDesc(nm, desc, NULL/*expr*/);
      prof.metricMgr()->insert(m_new);
      DIAG_Assert(m_new->id() >= numMetrics_orig, "Currently, we assume new metrics are added at the end of the metric vector.");
      metric_dst.push_back(m_new->id());
    }
  }

  // ------------------------------------------------------------
  // Create overhead metric values
  // ------------------------------------------------------------
  make(prof.cct()->root(), metric_src, metric_dst, false);
}


void 
OverheadMetricFact::make(Prof::CCT::ANode* node, 
			 const std::vector<uint>& m_src, 
			 const std::vector<uint>& m_dst, 
			 bool isOverheadCtxt)
{
  if (!node) { return; }

  // ------------------------------------------------------------
  // Visit CCT::Stmt nodes (Assumes metrics are only at leaves)
  // ------------------------------------------------------------
  if (isOverheadCtxt && (typeid(*node) == typeid(Prof::CCT::Stmt))) {
    Prof::CCT::Stmt* stmt = static_cast<Prof::CCT::Stmt*>(node);
    for (uint i = 0; i < m_src.size(); ++i) {
      uint src_idx = m_src[i];
      uint dst_idx = m_dst[i];

      if (stmt->hasMetric(src_idx)) {
	double mval = stmt->metric(src_idx);
	stmt->demandMetric(dst_idx) += mval;
	stmt->metric(src_idx) = 0.0;
      }
    }
  }

  // ------------------------------------------------------------
  // Recur
  // ------------------------------------------------------------
  
  // Note: once set, isOverheadCtxt should remain true for all descendents
  bool isOverheadCtxt_nxt = isOverheadCtxt;
  if (!isOverheadCtxt && typeid(*node) == typeid(Prof::CCT::ProcFrm)
      /* TODO: or Prof::CCT::Proc */) {
    Prof::CCT::ProcFrm* x = static_cast<Prof::CCT::ProcFrm*>(node);
    isOverheadCtxt_nxt = isOverhead(x);
  }

  for (Prof::CCT::ANodeChildIterator it(node); it.Current(); ++it) {
    Prof::CCT::ANode* x = it.current();
    make(x, m_src, m_dst, isOverheadCtxt_nxt);
  }
}


void
OverheadMetricFact::convertToWorkMetric(Prof::Metric::ADesc* mdesc)
{
  const string& nm = mdesc->name();
  if (nm.find("PAPI_TOT_CYC") == 0) {
    mdesc->nameBase("work (cyc)");
  }
  else if (nm.find("WALLCLOCK") == 0) {
    mdesc->nameBase("work (us)");
  }
  else {
    DIAG_Die(DIAG_Unimplemented);
  }
}


//***************************************************************************
//
//***************************************************************************

const string PthreadOverheadMetricFact::s_tag = "/libpthread";


bool 
PthreadOverheadMetricFact::isOverhead(const Prof::CCT::ProcFrm* x)
{
  const string& x_lm_nm = x->lmName();
  if (x_lm_nm.length() >= s_tag.length()) {
    return (x_lm_nm.find(s_tag) != string::npos);
  }
  return false;
}


//***************************************************************************
//
//***************************************************************************

const string CilkOverheadMetricFact::s_tag = "lush:parallel-overhead";


bool 
CilkOverheadMetricFact::isOverhead(const Prof::CCT::ProcFrm* x)
{
  const string& x_fnm = x->fileName();
  if (x_fnm.length() >= s_tag.length()) {
    size_t tag_beg = x_fnm.length() - s_tag.length();
    return (x_fnm.compare(tag_beg, s_tag.length(), s_tag) == 0);
  }
  return false;
}



} // namespace CallPath

} // namespace Analysis

//***************************************************************************
