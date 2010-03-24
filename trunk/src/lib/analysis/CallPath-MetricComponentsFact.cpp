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

//************************* System Include Files ****************************

#include <iostream>

#include <string>
using std::string;

#include <typeinfo>


//*************************** User Include Files ****************************

#include "CallPath-MetricComponentsFact.hpp"

#include <lib/support/diagnostics.h>
#include <lib/support/Logic.hpp>

//*************************** Forward Declarations ***************************


//****************************************************************************
// 
//****************************************************************************

namespace Analysis {

namespace CallPath {

//***************************************************************************


// Assumes: metrics are of type Metric::SampledDesc and values are
// only at leaves (CCT::Stmt)
void
MetricComponentsFact::make(Prof::CallPath::Profile& prof)
{
  using namespace Prof;
  
  // ------------------------------------------------------------
  // Create destination metric descriptors and mapping from source
  //   metrics to destination metrics
  // ------------------------------------------------------------
  std::vector<uint> metric_src;
  std::vector<uint> metric_dst;
  
  Metric::Mgr* metricMgr = prof.metricMgr();

  uint numMetrics_orig = metricMgr->size();
  for (uint mId = 0; mId < numMetrics_orig; ++mId) {
    Metric::ADesc* mDesc = metricMgr->metric(mId);
    if (MetricComponentsFact::isTimeMetric(mDesc)) {
      DIAG_Assert(typeid(*mDesc) == typeid(Metric::SampledDesc), DIAG_UnexpectedInput << "temporary sanity check");
      
      MetricComponentsFact::convertToWorkMetric(mDesc);
      metric_src.push_back(mId);

      Metric::ADesc* mDesc_new = mDesc->clone();
      mDesc_new->nameBase("overhead");
      mDesc_new->description("parallel overhead");

      metricMgr->insert(mDesc_new);
      DIAG_Assert(mDesc_new->id() >= numMetrics_orig, "Currently, we assume new metrics are added at the end of the metric vector.");
      
      metric_dst.push_back(mDesc_new->id());
    }
  }

  if (metric_src.empty()) {
    return;
  }

  // ------------------------------------------------------------
  // Create values for metric components
  // ------------------------------------------------------------
  make(prof.cct()->root(), metric_src, metric_dst, false);
}


// make: Assumes source metrics are of type "raw" Metric::SampledDesc,
// which means that metric values are exclusive and are only at leaves
// (CCT::Stmt).
//
// Since metric values are exclusive we effectively move metric values
// for whole subtrees.
void
MetricComponentsFact::make(Prof::CCT::ANode* node, 
			   const std::vector<uint>& m_src, 
			   const std::vector<uint>& m_dst, 
			   bool isSeparableCtxt)
{
  if (!node) { return; }

  // ------------------------------------------------------------
  // Visit CCT::Stmt nodes:
  // ------------------------------------------------------------
  if (isSeparableCtxt && (typeid(*node) == typeid(Prof::CCT::Stmt))) {
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
  
  // Note: once set, isSeparableCtxt should remain true for all descendents
  bool isSeparableCtxt_nxt = isSeparableCtxt;
  if (!isSeparableCtxt && typeid(*node) == typeid(Prof::CCT::ProcFrm)
      /* TODO: || typeid(Prof::CCT::Proc) */) {
    Prof::CCT::ProcFrm* x = static_cast<Prof::CCT::ProcFrm*>(node);
    isSeparableCtxt_nxt = isSeparable(x);
  }

  for (Prof::CCT::ANodeChildIterator it(node); it.Current(); ++it) {
    Prof::CCT::ANode* x = it.current();
    make(x, m_src, m_dst, isSeparableCtxt_nxt);
  }
}


void
MetricComponentsFact::convertToWorkMetric(Prof::Metric::ADesc* mdesc)
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

const string MPIBlameShiftIdlenessFact::s_tag = "MPIDI_CRAY_Progress_wait";


bool
MPIBlameShiftIdlenessFact::isSeparable(const Prof::CCT::ProcFrm* x)
{
  const string& x_nm = x->name();
  if (x_nm.length() >= s_tag.length()) {
    return (x_nm.find(s_tag) != string::npos);
  }
  return false;
}


// make: ...temporary holding pattern...
void
MPIBlameShiftIdlenessFact::make(Prof::CallPath::Profile& prof)
{
  using namespace Prof;

  // ------------------------------------------------------------
  // Create destination metric descriptors and mapping from source
  //   metrics to destination metrics
  // ------------------------------------------------------------
  std::vector<uint> metric_src;
  std::vector<uint> metric_dst;
  uint metric_balance = Metric::Mgr::npos;

  Metric::Mgr* metricMgr = prof.metricMgr();

  uint numMetrics_orig = metricMgr->size();
  for (uint mId = 0; mId < numMetrics_orig; ++mId) {
    Metric::ADesc* mDesc = metricMgr->metric(mId);

    // find main source metric
    if (MetricComponentsFact::isTimeMetric(mDesc)
	&& MetricComponentsFact::isSumMetric(mDesc)
	&& mDesc->type() == Metric::ADesc::TyIncl) {
      DIAG_Assert(mDesc->isComputed(), DIAG_UnexpectedInput);
      
      Metric::ADesc* mDesc_new = mDesc->clone();
      mDesc_new->nameBase("idleness");
      mDesc_new->description("MPI idleness");

      metricMgr->insert(mDesc_new);
      DIAG_Assert(mDesc_new->id() >= numMetrics_orig, "Currently, we assume new metrics are added at the end of the metric vector.");
      
      metric_dst.push_back(mDesc_new->id());
    }

    // find secondary source metric
    if (MetricComponentsFact::isTimeMetric(mDesc)
	&& MetricComponentsFact::isCoefVarMetric(mDesc)
	&& mDesc->type() == Metric::ADesc::TyIncl) {
      DIAG_Assert(mDesc->isComputed(), DIAG_UnexpectedInput);
      metric_balance = mDesc->id();
    }
  }

  if (metric_src.empty()) {
    return;
  }
  
  // ------------------------------------------------------------
  // Create values for metric components
  // ------------------------------------------------------------

  // prof.cct()->root()->aggregateMetricsIncl(...);
}


//***************************************************************************
//
//***************************************************************************

const string PthreadOverheadMetricFact::s_tag = "/libpthread";


bool
PthreadOverheadMetricFact::isSeparable(const Prof::CCT::ProcFrm* x)
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
CilkOverheadMetricFact::isSeparable(const Prof::CCT::ProcFrm* x)
{
  const string& x_fnm = x->fileName();
  if (x_fnm.length() >= s_tag.length()) {
    size_t tag_beg = x_fnm.length() - s_tag.length();
    return (x_fnm.compare(tag_beg, s_tag.length(), s_tag) == 0);
  }
  return false;
}


//***************************************************************************

} // namespace CallPath

} // namespace Analysis

//***************************************************************************
