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

//************************ System Include Files ******************************

#include <iostream>

#include <string>
using std::string;

#include <map>
using std::map;

#include <vector>
using std::vector;

#include <typeinfo>

//************************* User Include Files *******************************

#include "Metric-Mgr.hpp"

#include "Flat-ProfileData.hpp"

#include <lib/support/diagnostics.h>
#include <lib/support/StrUtil.hpp>


//************************ Forward Declarations ******************************


namespace Prof {

namespace Metric {

//****************************************************************************
//
//****************************************************************************

Mgr::Mgr()
{
}


Mgr::~Mgr()
{
  for (uint i = 0; i < m_metrics.size(); ++i) {
    delete m_metrics[i];
  }
}

//****************************************************************************

void
Mgr::makeRawMetrics(const std::vector<std::string>& profileFiles,
		    bool isUnitsEvents, bool doDispPercent)
{
  // ------------------------------------------------------------
  // Create a Metric::SampledDesc for each event within each profile
  // ------------------------------------------------------------
  for (uint i = 0; i < profileFiles.size(); ++i) {
    const string& proffnm = profileFiles[i];

    Prof::Flat::ProfileData prof;
    try {
      prof.open(proffnm.c_str());
    }
    catch (...) {
      DIAG_EMsg("While reading '" << proffnm << "'");
      throw;
    }

    const Metric::SampledDescVec& mdescs = prof.mdescs();
    for (uint j = 0; j < mdescs.size(); ++j) {
      const Metric::SampledDesc& mSmpl = *mdescs[j];

      bool isSortKey = (empty());
      
      Metric::SampledDesc* m = new Metric::SampledDesc(mSmpl);
      m->isSortKey(isSortKey);
      m->doDispPercent(doDispPercent);
      m->isUnitsEvents(isUnitsEvents);
      m->zeroDBInfo();

      insert(m);
    }
  }
}


uint
Mgr::makeSummaryMetrics(bool needAllStats, bool needMultiOccurance,
                        uint srcBegId, uint srcEndId)
{
  StringToADescVecMap nmToMetricMap;

  std::vector<Metric::ADescVec*> metricGroups;

  if (srcBegId == Mgr::npos) {
    srcBegId = 0;
  }
  if (srcEndId == Mgr::npos) {
    srcEndId = m_metrics.size();
  }

  uint threshold = (needMultiOccurance) ? 2 : 1;

  // -------------------------------------------------------
  // collect like metrics
  // -------------------------------------------------------
  for (uint i = srcBegId; i < srcEndId; ++i) {
    Metric::ADesc* m = m_metrics[i];
    string nm = m->nameGeneric();

    StringToADescVecMap::iterator it = nmToMetricMap.find(nm);
    if (it != nmToMetricMap.end()) {
      Metric::ADescVec& mvec = it->second;
      mvec.push_back(m);
    }
    else {
      std::pair<StringToADescVecMap::iterator, bool> ret =
	nmToMetricMap.insert(make_pair(nm, Metric::ADescVec(1, m)));
      Metric::ADescVec* grp = &(ret.first->second);
      metricGroups.push_back(grp);
    }
  }

  // -------------------------------------------------------
  // create summary metrics
  // -------------------------------------------------------
  uint firstId = Mgr::npos;

  for (uint i = 0; i < metricGroups.size(); ++i) {
    const Metric::ADescVec& mVec = *(metricGroups[i]);
    if (mVec.size() >= threshold) {
      const Metric::ADesc* m = mVec[0];

      Metric::ADesc* mNew =
	makeSummaryMetric("Sum",  m, mVec);

      if (needAllStats) {
        makeSummaryMetric("Mean",   m, mVec);
        makeSummaryMetric("StdDev", m, mVec);
        makeSummaryMetric("CfVar",  m, mVec);
        makeSummaryMetric("Min",    m, mVec);
        makeSummaryMetric("Max",    m, mVec);
      }

      if (firstId == Mgr::npos) {
	firstId = mNew->id();
      }
    }
  }

  computePartners();

  return firstId;
}


uint
Mgr::makeSummaryMetricsIncr(bool needAllStats, uint srcBegId, uint srcEndId)
{
  if (srcBegId == Mgr::npos) {
    srcBegId = 0;
  }
  if (srcEndId == Mgr::npos) {
    srcEndId = m_metrics.size();
  }

  uint firstId = Mgr::npos;

  for (uint i = srcBegId; i < srcEndId; ++i) {
    Metric::ADesc* m = m_metrics[i];

    Metric::ADesc* mNew =
      makeSummaryMetricIncr("Sum",  m);

    if (needAllStats) {
      makeSummaryMetricIncr("Mean",   m);
      makeSummaryMetricIncr("StdDev", m);
      makeSummaryMetricIncr("CfVar",  m);
      makeSummaryMetricIncr("Min",    m);
      makeSummaryMetricIncr("Max",    m);
    }
    
    if (firstId == Mgr::npos) {
      firstId = mNew->id();
    }
  }

  computePartners();
 
  return firstId;
}


Metric::DerivedDesc*
Mgr::makeSummaryMetric(const string mDrvdTy, const Metric::ADesc* mSrc,
		       const Metric::ADescVec& mOpands)
{
  Metric::AExpr** opands = new Metric::AExpr*[mOpands.size()];
  for (uint i = 0; i < mOpands.size(); ++i) {
    Metric::ADesc* m = mOpands[i];
    opands[i] = new Metric::Var(m->name(), m->id());
  }

  bool doDispPercent = true;
  bool isPercent = false;
  bool isVisible = true;

  // This is a cheesy way of creating the metrics, but it is good
  // enough for now.

  Metric::AExpr* expr = NULL;
  if (mDrvdTy.find("Sum", 0) == 0) {
    expr = new Metric::Plus(opands, mOpands.size());
  }
  else if (mDrvdTy.find("Mean", 0) == 0) {
    expr = new Metric::Mean(opands, mOpands.size());
    doDispPercent = false;
  }
  else if (mDrvdTy.find("StdDev", 0) == 0) {
    expr = new Metric::StdDev(opands, mOpands.size());
    doDispPercent = false;
  }
  else if (mDrvdTy.find("CfVar", 0) == 0) {
    expr = new Metric::CoefVar(opands, mOpands.size());
    doDispPercent = false;
  }
  else if (mDrvdTy.find("%CfVar", 0) == 0) {
    expr = new Metric::RStdDev(opands, mOpands.size());
    isPercent = true;
  }
  else if (mDrvdTy.find("Min", 0) == 0) {
    expr = new Metric::Min(opands, mOpands.size());
    doDispPercent = false;
  }
  else if (mDrvdTy.find("Max", 0) == 0) {
    expr = new Metric::Max(opands, mOpands.size());
    doDispPercent = false;
  }
  else {
    DIAG_Die(DIAG_UnexpectedInput);
  }
  
  string mNmFmt = mSrc->nameToFmt();
  string mNmBase = mSrc->nameBase() + ":" + mDrvdTy;
  const string& mDesc = mSrc->description();

  DerivedDesc* m =
    new DerivedDesc(mNmFmt, mDesc, expr, isVisible, true/*isSortKey*/,
		    doDispPercent, isPercent);
  m->nameBase(mNmBase);
  m->nameSfx(""); // clear; cf. Prof::CallPath::Profile::RFlg_NoMetricSfx
  m->zeroDBInfo(); // clear
  insert(m);
  expr->accumId(m->id());

  if (expr->hasAccum2()) {
    string m2NmBase = mNmBase + ":accum2";
    DerivedDesc* m2 =
      new DerivedDesc(mNmFmt, mDesc, NULL/*expr*/, false/*isVisible*/,
		      false/*isSortKey*/, false/*doDispPercent*/,
		      false/*isPercent*/);
    m2->nameBase(m2NmBase);
    m2->nameSfx(""); // clear; cf. Prof::CallPath::Profile::RFlg_NoMetricSfx
    m2->zeroDBInfo(); // clear
    insert(m2);

    expr->accum2Id(m2->id());
  }

  if (expr->hasNumSrcVar()) {
    string m3NmBase = mNmBase + ":num-src";
    Metric::NumSource* m3Expr = new Metric::NumSource(mOpands.size());
    DerivedDesc* m3 =
      new DerivedDesc(mNmFmt, mDesc, m3Expr, false/*isVisible*/,
		      false/*isSortKey*/, false/*doDispPercent*/,
		      false/*isPercent*/);
    m3->nameBase(m3NmBase);
    m3->nameSfx(""); // clear; cf. Prof::CallPath::Profile::RFlg_NoMetricSfx
    m3->zeroDBInfo(); // clear
    insert(m3);
    m3Expr->accumId(m3->id());

    expr->numSrcVarId(m3->id());
  }

  return m;
}


Metric::DerivedIncrDesc*
Mgr::makeSummaryMetricIncr(const string mDrvdTy, const Metric::ADesc* mSrc)
{
  bool doDispPercent = true;
  bool isPercent = false;
  bool isVisible = true;

  // This is a cheesy way of creating the metrics, but it is good
  // enough for now.

  Metric::AExprIncr* expr = NULL;
  if (mDrvdTy.find("Sum", 0) == 0) {
    expr = new Metric::SumIncr(Metric::IData::npos, mSrc->id());
  }
  else if (mDrvdTy.find("Mean", 0) == 0) {
    expr = new Metric::MeanIncr(Metric::IData::npos, mSrc->id());
    doDispPercent = false;
  }
  else if (mDrvdTy.find("StdDev", 0) == 0) {
    expr = new Metric::StdDevIncr(Metric::IData::npos, 0, mSrc->id());
    doDispPercent = false;
  }
  else if (mDrvdTy.find("CfVar", 0) == 0) {
    expr = new Metric::CoefVarIncr(Metric::IData::npos, 0, mSrc->id());
    doDispPercent = false;
  }
  else if (mDrvdTy.find("%CfVar", 0) == 0) {
    expr = new Metric::RStdDevIncr(Metric::IData::npos, 0, mSrc->id());
    isPercent = true;
  }
  else if (mDrvdTy.find("Min", 0) == 0) {
    expr = new Metric::MinIncr(Metric::IData::npos, mSrc->id());
    doDispPercent = false;
  }
  else if (mDrvdTy.find("Max", 0) == 0) {
    expr = new Metric::MaxIncr(Metric::IData::npos, mSrc->id());
    doDispPercent = false;
  }
  else {
    DIAG_Die(DIAG_UnexpectedInput);
  }
  
  string mNmFmt = mSrc->nameToFmt();
  string mNmBase = mSrc->nameBase() + ":" + mDrvdTy;
  const string& mDesc = mSrc->description();

  DerivedIncrDesc* m =
    new DerivedIncrDesc(mNmFmt, mDesc, expr, isVisible,
			true/*isSortKey*/, doDispPercent, isPercent);
  m->nameBase(mNmBase);
  m->zeroDBInfo(); // clear
  insert(m);
  expr->accumId(m->id());

  if (expr->hasAccum2()) {
    string m2NmBase = mNmBase + ":accum2";
    DerivedIncrDesc* m2 =
      new DerivedIncrDesc(mNmFmt, mDesc, NULL/*expr*/, false/*isVisible*/,
			  false/*isSortKey*/, false/*doDispPercent*/,
			  false/*isPercent*/);
    m2->nameBase(m2NmBase);
    m2->zeroDBInfo(); // clear
    insert(m2);

    expr->accum2Id(m2->id());
  }

  if (expr->hasNumSrcVar()) {
    string m3NmBase = mNmBase + ":num-src";
    Metric::NumSourceIncr* m3Expr = new Metric::NumSourceIncr(0, mSrc->id());
    DerivedIncrDesc* m3 =
      new DerivedIncrDesc(mNmFmt, mDesc, m3Expr, false/*isVisible*/,
			  false/*isSortKey*/, false/*doDispPercent*/,
			  false/*isPercent*/);
    m3->nameBase(m3NmBase);
    m3->zeroDBInfo(); // clear
    insert(m3);
    m3Expr->accumId(m3->id());

    expr->numSrcVarId(m3->id());
  }

  return m;
}


//****************************************************************************

bool
Mgr::insert(Metric::ADesc* m)
{
  // 1. metric table
  uint id = m_metrics.size();
  m_metrics.push_back(m);
  m->id(id);

  // 2. insert in maps and ensure name is unique
  bool changed = insertInMapsAndMakeUniqueName(m);
  return changed;
}


bool
Mgr::insertIf(Metric::ADesc* m)
{
  string nm = m->name();
  if (metric(nm)) {
    return false; // already exists
  }

  insert(m);
  return true;
}


Metric::ADesc*
Mgr::findSortKey() const
{
  Metric::ADesc* found = NULL;
  for (uint i = 0; i < m_metrics.size(); ++i) {
    Metric::ADesc* m = m_metrics[i];
    if (m->isSortKey()) {
      found = m;
      break;
    }
  }
  return found;
}


Metric::ADesc*
Mgr::findFirstVisible() const
{
  Metric::ADesc* found = NULL;
  for (uint i = 0; i < m_metrics.size(); ++i) {
    Metric::ADesc* m = m_metrics[i];
    if (m->isVisible()) {
      found = m;
      break;
    }
  }
  return found;
}


Metric::ADesc*
Mgr::findLastVisible() const
{
  Metric::ADesc* found = NULL;
  for (int i = m_metrics.size() - 1; i >= 0; --i) { // i may be < 0
    Metric::ADesc* m = m_metrics[i];
    if (m->isVisible()) {
      found = m;
      break;
    }
  }
  return found;
}


bool
Mgr::hasDerived() const
{
  for (uint i = 0; i < m_metrics.size(); ++i) {
    Metric::ADesc* m = m_metrics[i];
    if (typeid(*m) == typeid(Prof::Metric::DerivedDesc) ||
	typeid(*m) == typeid(Prof::Metric::DerivedIncrDesc)) {
      return true;
    }
  }
  return false;
}


//****************************************************************************

// findGroup: see general comments in header.
//
// Currently assumes:
// 1. Metric groups in both x and y appear in sorted order and without
//    gaps.  This assumption is upheld because (a) groups are
//    distributed across processes in sorted order and (b) the
//    reduction tree always merges left before right children.
//
//    This means we should *never* see:
//      x: [2.a 2.b]  y: [1.a 1.b | 2.a 2.b]
//      x: [2.a 2.b]  y: [1.a 1.b | 3.a 3.b]
//
// 2. All profile files in a group have the same set of metrics.
// 
// While this simplifies things, because of groups, we still have to
// merge profiles where only a subgroup of y matches a group in x.
//   x: [1.a 1.b]  y: [1.a 1.b | 2.a 2.b]
// 
// 
// *** N.B.: *** Assumptions (1) and (2) imply that either (a) y's
// metrics fully match x's or (b) y's *first* metric subgroup fully
// matches x's metrics.  It also enables us to use a metric's unique
// name for a search.
// 
//
// TODO: Eventually, we cannot assume (2).  It will be possible for
// a group to have different sets of metrics, which could lead to
// merges like the following:
//
//    x: [1.a 1.b]            y: [1.a 1.b | 1.a 1.c]
//    x: [1.a 1.c]            y: [1.a 1.b | 1.a 1.c]
//    x: [1.a 1.c] [1.d 1.e]  y: [1.a 1.b | 1.a 1.c]
//
// This implies that *any* subgroup of y could match *any* subgroup of
// x.  It also implies that we cannot search by a metric's unique name
// because the unique name in y may be different than the unique name
// in x.
//
uint
Mgr::findGroup(const Mgr& y) const
{
  const Mgr* x = this;

  // -------------------------------------------------------
  // Based on observation above, we first try to match y's first
  // subgroup.
  // -------------------------------------------------------

  uint y_grp_sz = 0; // open end boundary
  if (y.size() > 0) {
    y_grp_sz = 1; // metric 0 is first entry in 'y_grp'
    const string& y_grp_pfx = y.metric(0)->namePfx();

    for (uint y_i = 1; y_i < y.size(); ++y_i) {
      const string& mPfx = y.metric(y_i)->namePfx();
      if (mPfx != y_grp_pfx) {
	break;
      }
      y_grp_sz++;
    }
  }

  bool found = true; // optimistic

  std::vector<uint> metricMap(y_grp_sz);
  
  for (uint y_i = 0; y_i < y_grp_sz; ++y_i) {
    const Metric::ADesc* y_m = y.metric(y_i);
    string mNm = y_m->name();
    const Metric::ADesc* x_m = x->metric(mNm);
    
    if (!x_m || (y_i > 0 && x_m->id() != (metricMap[y_i - 1] + 1))) {
      found = false;
      break;
    }
    
    metricMap[y_i] = x_m->id();
  }
  
  bool foundGrp = (found && !metricMap.empty());

  // -------------------------------------------------------
  // 
  // -------------------------------------------------------

  if (foundGrp) {
    // sanity check: either (x.size() == y_grp_sz) or the rest of x
    // matches the rest of y.
    for (uint x_i = metricMap[y_grp_sz - 1] + 1, y_i = y_grp_sz;
	 x_i < x->size() && y_i < y.size(); ++x_i, ++y_i) {
      DIAG_Assert(x->metric(x_i)->name() == y.metric(y_i)->name(), "");
    }
  }

  return (foundGrp) ? metricMap[0] : Mgr::npos;
}


//****************************************************************************

void
Mgr::recomputeMaps()
{
  // clear maps
  m_nuniqnmToMetricMap.clear();
  m_uniqnmToMetricMap.clear();
  m_fnameToFMetricMap.clear();

  for (uint i = 0; i < m_metrics.size(); ++i) {
    Metric::ADesc* m = m_metrics[i];
    insertInMapsAndMakeUniqueName(m);
  }
}


void
Mgr::computePartners()
{
  StringToADescMap metricsIncl;
  StringToADescMap metricsExcl;

  // -------------------------------------------------------
  // populate maps
  // -------------------------------------------------------
  for (uint i = 0; i < m_metrics.size(); ++i) {
    Metric::ADesc* m = m_metrics[i];
    string nm = m->namePfxBaseSfx();

    StringToADescMap* metricsMap = NULL;
    switch (m->type()) {
      case ADesc::TyIncl: metricsMap = &metricsIncl; break;
      case ADesc::TyExcl: metricsMap = &metricsExcl; break;
      default: break;
    }
    
    if (metricsMap) {
      DIAG_MsgIf(0, "Metric::Mgr::computePartners: insert: " << nm
		 << " [" << m->name() << "]");
      std::pair<StringToADescMap::iterator, bool> ret =
	metricsMap->insert(make_pair(nm, m));
      DIAG_Assert(ret.second, "Metric::Mgr::computePartners: Found duplicate entry inserting:\n\t" << m->toString() << "\nOther entry:\n\t" << ret.first->second->toString());
    }
  }

  // -------------------------------------------------------
  // find partners
  // -------------------------------------------------------
  for (uint i = 0; i < m_metrics.size(); ++i) {
    Metric::ADesc* m = m_metrics[i];
    string nm = m->namePfxBaseSfx();

    StringToADescMap* metricsMap = NULL;
    switch (m->type()) {
      case ADesc::TyIncl: metricsMap = &metricsExcl; break;
      case ADesc::TyExcl: metricsMap = &metricsIncl; break;
      default: break;
    }
    
    if (metricsMap) {
      StringToADescMap::iterator it = metricsMap->find(nm);
      if (it != metricsMap->end()) {
	Metric::ADesc* partner = it->second;
	m->partner(partner);
	DIAG_MsgIf(0, "Metric::Mgr::computePartners: found: "
		   << m->name() << " -> " << partner->name());
      }
    }
  }
}


//****************************************************************************

void
Mgr::zeroDBInfo() const
{
  for (uint i = 0; i < m_metrics.size(); ++i) {
    Metric::ADesc* m = m_metrics[i];
    if (m->hasDBInfo()) {
      m->zeroDBInfo();
    }
  }
}


//****************************************************************************

string
Mgr::toString(const char* pfx) const
{
  std::ostringstream os;
  dump(os, pfx);
  return os.str();
}


std::ostream&
Mgr::dump(std::ostream& os, const char* pfx) const
{
  os << pfx << "[ metric table:" << std::endl;
  for (uint i = 0; i < m_metrics.size(); i++) {
    Metric::ADesc* m = m_metrics[i];
    os << pfx << "  " << m->id() << ": " << m->toString() << std::endl;
  }
  os << pfx << "]" << std::endl;

  os << pfx << "[ unique-name-to-metric:" << std::endl;
  for (StringToADescMap::const_iterator it = m_uniqnmToMetricMap.begin();
       it !=  m_uniqnmToMetricMap.end(); ++it) {
    const string& nm = it->first;
    Metric::ADesc* m = it->second;
    os << pfx << "  " << nm << " -> " << m->toString() << std::endl;
  }
  os << pfx << "]" << std::endl;

  return os;
}


void
Mgr::ddump() const
{
  dump(std::cerr);
  std::cerr.flush();
}


//****************************************************************************
//
//****************************************************************************

bool
Mgr::insertInMapsAndMakeUniqueName(Metric::ADesc* m)
{
  bool isChanged = false;

  // 1. metric name to Metric::ADescVec table
  string nm = m->name();
  StringToADescVecMap::iterator it = m_nuniqnmToMetricMap.find(nm);
  if (it != m_nuniqnmToMetricMap.end()) {
    Metric::ADescVec& mvec = it->second;

    // ensure uniqueness: qualifier is an integer >= 1
    int qualifier = mvec.size();
    const string& nm_sfx = m->nameSfx();
    string sfx_new = nm_sfx;
    if (!sfx_new.empty()) {
      sfx_new += ".";
    }
    sfx_new += StrUtil::toStr(qualifier);
    
    m->nameSfx(sfx_new);
    nm = m->name(); // update 'nm'
    isChanged = true;

    mvec.push_back(m);
  }
  else {
    m_nuniqnmToMetricMap.insert(make_pair(nm, Metric::ADescVec(1, m)));
  }

  // 2. unique name to Metric::ADesc table
  std::pair<StringToADescMap::iterator, bool> ret =
    m_uniqnmToMetricMap.insert(make_pair(nm, m));
  DIAG_Assert(ret.second, "Metric::Mgr::insertInMapsAndMakeUniqueName: Found duplicate entry inserting:\n\t" << m->toString() << "\nOther entry:\n\t" << ret.first->second->toString());

  
  // 3. profile file name to Metric::SampledDesc table
  Metric::SampledDesc* mSmpl = dynamic_cast<Metric::SampledDesc*>(m);
  if (mSmpl) {
    const string& fnm = mSmpl->profileName();
    StringToADescVecMap::iterator it1 = m_fnameToFMetricMap.find(fnm);
    if (it1 != m_fnameToFMetricMap.end()) {
      Metric::ADescVec& mvec = it1->second;
      mvec.push_back(mSmpl);
    }
    else {
      m_fnameToFMetricMap.insert(make_pair(fnm, Metric::ADescVec(1, mSmpl)));
    }
  }

  return isChanged;
}


//****************************************************************************

} // namespace Metric

} // namespace Prof
