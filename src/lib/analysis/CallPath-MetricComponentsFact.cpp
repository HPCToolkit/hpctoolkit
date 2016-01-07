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

#include <typeinfo>


//*************************** User Include Files ****************************

#include "CallPath-MetricComponentsFact.hpp"

#include <lib/binutils/VMAInterval.hpp>

#include <lib/support/diagnostics.h>
#include <lib/support/Logic.hpp>

//*************************** Forward Declarations ***************************


//****************************************************************************
// 
//****************************************************************************

namespace Analysis {

namespace CallPath {

//***************************************************************************

const std::string MetricComponentsFact::s_sum   = ":Sum";
const std::string MetricComponentsFact::s_cfvar = ":CfVar";


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
  std::vector<uint> metricSrcIds;
  std::vector<uint> metricDstIds;
  
  Metric::Mgr* metricMgr = prof.metricMgr();

  uint numMetrics_orig = metricMgr->size();
  for (uint mId = 0; mId < numMetrics_orig; ++mId) {
    Metric::ADesc* m = metricMgr->metric(mId);
    if (MetricComponentsFact::isTimeMetric(m)) {
      DIAG_Assert(typeid(*m) == typeid(Metric::SampledDesc), DIAG_UnexpectedInput << "temporary sanity check");
      
      MetricComponentsFact::convertToWorkMetric(m);
      metricSrcIds.push_back(m->id());

      Metric::ADesc* m_new = m->clone();
      m_new->nameBase("overhead");
      m_new->description("parallel overhead");

      metricMgr->insert(m_new);
      DIAG_Assert(m_new->id() >= numMetrics_orig, "Currently, we assume new metrics are added at the end of the metric vector.");
      
      metricDstIds.push_back(m_new->id());
    }
  }

  if (metricSrcIds.empty()) {
    return;
  }

  // ------------------------------------------------------------
  // Create values for metric components
  // ------------------------------------------------------------
  make(prof.cct()->root(), metricSrcIds, metricDstIds, false);
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
      uint mId_src = m_src[i];
      uint mId_dst = m_dst[i];

      if (stmt->hasMetric(mId_src)) {
	double mval = stmt->metric(mId_src);
	stmt->demandMetric(mId_dst) += mval;
	stmt->metric(mId_src) = 0.0;
      }
    }
  }

  // ------------------------------------------------------------
  // Recur
  // ------------------------------------------------------------
  
  // Note: once set, isSeparableCtxt should remain true for all descendents
  bool isSeparableCtxt_nxt = isSeparableCtxt;
  if (!isSeparableCtxt && dynamic_cast<Prof::CCT::AProcNode*>(node)) {
    Prof::CCT::AProcNode* x = static_cast<Prof::CCT::AProcNode*>(node);
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

// Cray XT4, XT5
const string MPIBlameShiftIdlenessFact::s_tag1 = "MPIDI_CRAY_Progress_wait";

// IBM BG/P
const string MPIBlameShiftIdlenessFact::s_tag2 = "MPID_Progress_wait";

// Intel/Infiniband
const string MPIBlameShiftIdlenessFact::s_tag3 = "MPIDI_CH3I_Progress";


static bool
isMPIFrame(const Prof::CCT::ProcFrm* x)
{
  static const string mpistr1 = "MPI";
  static const string mpistr2 = "PMPI";
  const string& x_nm = x->procName();
  return (x_nm.compare(0, mpistr1.length(), mpistr1) == 0 ||
	  x_nm.compare(0, mpistr2.length(), mpistr2) == 0);
}


bool
MPIBlameShiftIdlenessFact::isSeparable(const Prof::CCT::AProcNode* x)
{
  const string& x_nm = x->procName();
  bool fnd = (x_nm.find(s_tag1) != string::npos ||
	      x_nm.find(s_tag2) != string::npos ||
	      x_nm.find(s_tag3) != string::npos);
  return fnd;
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
  std::vector<uint> metricSrcIds;
  std::vector<uint> metricBalanceIds;
  std::vector<uint> metricImbalInclIds, metricImbalExclIds;
  std::vector<uint> metricIdleInclIds;

  Metric::Mgr* metricMgr = prof.metricMgr();

  uint numMetrics_orig = metricMgr->size();
  for (uint mId = 0; mId < numMetrics_orig; ++mId) {
    Metric::ADesc* m = metricMgr->metric(mId);

    // find main source metric
    if (MetricComponentsFact::isTimeMetric(m)
	&& MetricComponentsFact::isDerivedMetric(m, s_sum)
	&& m->type() == Metric::ADesc::TyIncl
	&& m->isVisible() /* not a temporary */) {

      DIAG_Assert(m->computedType() == Prof::Metric::ADesc::ComputedTy_NonFinal,
		  DIAG_UnexpectedInput);
      metricSrcIds.push_back(m->id());

      // FIXME: For now we use only Metric::ADesc::DerivedIncrDesc()
      //   We should also support Metric::ADesc::DerivedDesc()
      DIAG_Assert(typeid(*m) == typeid(Metric::DerivedIncrDesc), DIAG_UnexpectedInput);

      Metric::DerivedIncrDesc* m_imbalIncl =
	static_cast<Metric::DerivedIncrDesc*>(m->clone());
      m_imbalIncl->nameBase("imbalance" + s_sum);
      m_imbalIncl->description("imbalance for MPI SPMD executions");
      m_imbalIncl->expr(new Metric::SumIncr(Metric::IData::npos, // FIXME:Sum
					    Metric::IData::npos));

      Metric::DerivedIncrDesc* m_imbalExcl =
	static_cast<Metric::DerivedIncrDesc*>(m_imbalIncl->clone());
      m_imbalExcl->type(Metric::ADesc::TyExcl);
      m_imbalExcl->expr(new Metric::SumIncr(Metric::IData::npos,
					    Metric::IData::npos));
      m_imbalIncl->partner(m_imbalExcl);
      m_imbalExcl->partner(m_imbalIncl);

      Metric::DerivedIncrDesc* m_idleIncl =
	static_cast<Metric::DerivedIncrDesc*>(m->clone());
      m_idleIncl->nameBase("idleness" + s_sum);
      m_idleIncl->description("idleness for MPI executions");
      m_idleIncl->partner(NULL);
      m_idleIncl->expr(new Metric::SumIncr(Metric::IData::npos, // FIXME:Sum
					   Metric::IData::npos));

      metricMgr->insert(m_imbalIncl);
      metricMgr->insert(m_imbalExcl);
      metricMgr->insert(m_idleIncl);

      m_imbalIncl->expr()->accumId(m_imbalIncl->id());
      m_imbalExcl->expr()->accumId(m_imbalExcl->id());
      m_idleIncl->expr()->accumId(m_idleIncl->id());

      DIAG_Assert(m_imbalIncl->id() >= numMetrics_orig && m_imbalExcl->id() >= numMetrics_orig, "Currently, we assume new metrics are added at the end of the metric vector.");
      
      metricImbalInclIds.push_back(m_imbalIncl->id());
      metricImbalExclIds.push_back(m_imbalExcl->id());
      metricIdleInclIds.push_back(m_idleIncl->id());
    }

    // find secondary source metric
    if (MetricComponentsFact::isTimeMetric(m)
	&& MetricComponentsFact::isDerivedMetric(m, s_cfvar)
	&& m->type() == Metric::ADesc::TyIncl
	&& m->isVisible() /* not a temporary */) {
      DIAG_Assert(m->computedType() == Prof::Metric::ADesc::ComputedTy_NonFinal,
		  DIAG_UnexpectedInput);
      metricBalanceIds.push_back(m->id());
    }
  }

  DIAG_Assert(metricSrcIds.size() == metricBalanceIds.size(), DIAG_UnexpectedInput);

  if (metricSrcIds.empty()) {
    return;
  }
  
  // ------------------------------------------------------------
  // Create values for metric components
  // ------------------------------------------------------------

  // Note that metrics are non-finalized!
  CCT::ANode* cctRoot = prof.cct()->root();

  uint metricBalancedId = metricBalanceIds[0];
  Metric::AExprIncr* metricBalancedExpr = dynamic_cast<Metric::DerivedIncrDesc*>(metricMgr->metric(metricBalancedId))->expr();

  Metric::IData cctRoot_mdata(*cctRoot);
  metricBalancedExpr->finalize(cctRoot_mdata);
  
  double balancedThreshold = 1.2 * cctRoot_mdata.demandMetric(metricBalancedId);

  makeMetrics(cctRoot, metricSrcIds,
	      metricImbalInclIds, metricImbalExclIds, metricIdleInclIds,
	      metricBalancedId, metricBalancedExpr, balancedThreshold,
	      NULL, NULL);


  VMAIntervalSet metricDstInclIdSet;
  for (uint i = 0; i < metricImbalInclIds.size(); ++i) {
    uint mId = metricImbalInclIds[i];
    metricDstInclIdSet.insert(VMAInterval(mId, mId + 1)); // [ )

    mId = metricIdleInclIds[i];
    metricDstInclIdSet.insert(VMAInterval(mId, mId + 1)); // [ )
  }

  cctRoot->aggregateMetricsIncl(metricDstInclIdSet);
}


void
MPIBlameShiftIdlenessFact::makeMetrics(Prof::CCT::ANode* node,
				       const std::vector<uint>& m_src,
				       const std::vector<uint>& m_imbalIncl,
				       const std::vector<uint>& m_imbalExcl,
				       const std::vector<uint>& m_idleIncl,
				       uint mId_bal,
				       Prof::Metric::AExprIncr* balancedExpr,
				       double balancedThreshold,
				       Prof::CCT::ANode* balancedFrm,
				       Prof::CCT::ANode* balancedNode)
{
  using namespace Prof;

  if (!node) { return; }

  // -------------------------------------------------------
  // Shift blame for waiting at 'node' (use non-finalized metric values)
  // -------------------------------------------------------
  bool isFrame = (typeid(*node) == typeid(Prof::CCT::ProcFrm));

  if (isFrame && isSeparable(static_cast<Prof::CCT::ProcFrm*>(node))) {
    DIAG_Assert(balancedNode, DIAG_UnexpectedInput);

    CCT::ProcFrm* balancedNodeFrm = balancedNode->ancestorProcFrm();

    for (uint i = 0; i < m_src.size(); ++i) {
      uint mId_src = m_src[i];

      uint mId_imbalIncl = m_imbalIncl[i];
      uint mId_imbalExcl = m_imbalExcl[i];
      uint mId_idleIncl  = m_idleIncl[i];

      double mval = node->demandMetric(mId_src);

      balancedNode->demandMetric(mId_imbalIncl) += mval; // FIXME: combine fn
      balancedNode->demandMetric(mId_imbalExcl) += mval; // FIXME: combine fn

      if (balancedNode != balancedFrm && balancedNodeFrm == balancedFrm) {
	balancedFrm->demandMetric(mId_imbalExcl) += mval; // FIXME: combine fn
      }

      node->demandMetric(mId_idleIncl) += mval; // FIXME: combine fn
    }
    
    return; // do not recur down this subtree
  }

  // -------------------------------------------------------
  // Find balanced nodes (use finalized metric values)
  // -------------------------------------------------------
  Metric::IData node_mdata(*node);
  balancedExpr->finalize(node_mdata);
  
  bool isComp = (!isFrame ||
		 (/*isFrame &&*/
		  !isMPIFrame(static_cast<Prof::CCT::ProcFrm*>(node))));
  bool isBalanced = (node_mdata.demandMetric(mId_bal) <= balancedThreshold);

  CCT::ANode* balancedFrmNxt = ((isFrame && isBalanced && isComp) ?
				node : balancedFrm);
  CCT::ANode* balancedNodeNxt = (isBalanced && isComp) ? node : balancedNode;

  // ------------------------------------------------------------
  // Recur
  // ------------------------------------------------------------
  for (Prof::CCT::ANodeChildIterator it(node); it.Current(); ++it) {
    Prof::CCT::ANode* x = it.current();
    makeMetrics(x, m_src, m_imbalIncl, m_imbalExcl, m_idleIncl,
		mId_bal, balancedExpr, balancedThreshold,
		balancedFrmNxt, balancedNodeNxt);
  }
}


//***************************************************************************
//
//***************************************************************************

const string PthreadOverheadMetricFact::s_tag = "/libpthread";


bool
PthreadOverheadMetricFact::isSeparable(const Prof::CCT::AProcNode* x)
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
CilkOverheadMetricFact::isSeparable(const Prof::CCT::AProcNode* x)
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
