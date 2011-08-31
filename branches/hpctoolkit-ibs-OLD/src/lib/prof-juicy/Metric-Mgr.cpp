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

#include <lib/prof-juicy/Flat-ProfileData.hpp>

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

      insert(m);
    }
  }
}


uint 
Mgr::makeSummaryMetrics(uint srcBegId, uint srcEndId)
{
  StringToADescVecMap nmToMetricMap;

  if (srcBegId == Mgr::npos) {
    srcBegId = 0;
  }
  if (srcEndId == Mgr::npos) {
    srcEndId = m_metrics.size();
  }

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
      nmToMetricMap.insert(make_pair(nm, Metric::ADescVec(1, m)));
    }
  }

  // -------------------------------------------------------
  // create summary metrics
  // -------------------------------------------------------
  uint firstId = Mgr::npos;

  StringToADescVecMap::iterator it = nmToMetricMap.begin();
  for ( ; it != nmToMetricMap.end(); ++it) {
    const string& mNm = it->first;
    Metric::ADescVec& mvec = it->second;
    if (mvec.size() > 1) {
      Metric::ADesc* mNew = NULL;

      string mean_nm = "Mean-" + mNm;
      string cv_nm   = "CfVar-" + mNm;
      string min_nm  = "Min-" + mNm;
      string max_nm  = "Max-" + mNm;
      string sum_nm  = "Sum-" + mNm;

      mNew = makeSummaryMetric(mean_nm, mvec);
      makeSummaryMetric(cv_nm, mvec);
      makeSummaryMetric(min_nm, mvec);
      makeSummaryMetric(max_nm, mvec);
      makeSummaryMetric(sum_nm, mvec);

      if (firstId == Mgr::npos) {
	firstId = mNew->id();
      }
    }
  }

  return firstId;
}


uint
Mgr::makeSummaryMetricsIncr(uint srcBegId, uint srcEndId)
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
      makeSummaryMetricIncr("Mean", m);
    makeSummaryMetricIncr("CfVar",  m);
    makeSummaryMetricIncr("Min",    m);
    makeSummaryMetricIncr("Max",    m);
    makeSummaryMetricIncr("Sum",    m);
    
    if (firstId == Mgr::npos) {
      firstId = mNew->id();
    }
  }
  
  return firstId;
}


Metric::DerivedDesc*
Mgr::makeSummaryMetric(const string& mNm, const Metric::ADescVec& mOpands)
{
  Metric::AExpr** opands = new Metric::AExpr*[mOpands.size()];
  for (uint i = 0; i < mOpands.size(); ++i) {
    Metric::ADesc* m = mOpands[i];
    opands[i] = new Metric::Var(m->name(), m->id());
  }

  bool doDispPercent = true;
  bool isPercent = false;

  // This is a a cheesy way of creating the metrics, but it is good
  // enough for now.  Perhaps, this can be pushed into a metric parser
  // as mathxml is retired.
  Metric::AExpr* expr = NULL;
  if (mNm.find("Mean", 0) == 0) {
    expr = new Metric::Mean(opands, mOpands.size());
    doDispPercent = false;
  }
  else if (mNm.find("SDev", 0) == 0) {
    expr = new Metric::StdDev(opands, mOpands.size());
    doDispPercent = false;
  }
  else if (mNm.find("CfVar", 0) == 0) {
    expr = new Metric::CoefVar(opands, mOpands.size());
    doDispPercent = false;
  }
  else if (mNm.find("%CfVar", 0) == 0) {
    expr = new Metric::RStdDev(opands, mOpands.size());
    isPercent = true;
  }
  else if (mNm.find("Min", 0) == 0) {
    expr = new Metric::Min(opands, mOpands.size());
    doDispPercent = false;
  }
  else if (mNm.find("Max", 0) == 0) {
    expr = new Metric::Max(opands, mOpands.size());
    doDispPercent = false;
  }
  else if (mNm.find("Sum", 0) == 0) {
    expr = new Metric::Plus(opands, mOpands.size());
  }
  else {
    DIAG_Die(DIAG_UnexpectedInput);
  }
 
  DerivedDesc* m =
    new DerivedDesc(mNm, mNm, expr, true/*isVisible*/, true/*isSortKey*/,
		    doDispPercent, isPercent);
  insert(m);

  return m;
}


Metric::DerivedIncrDesc*
Mgr::makeSummaryMetricIncr(const string mDrvdTy, const Metric::ADesc* mSrc)
{
  bool doDispPercent = true;
  bool isPercent = false;

  // This is a cheesy way of creating the metrics, but 1) it is good
  // enough for now and 2) we don't yet have a clearly better plan.

  Metric::AExprIncr* expr = NULL;
  if (mDrvdTy.find("Mean", 0) == 0) {
    expr = new Metric::MeanIncr(0, mSrc->id());
    doDispPercent = false;
  }
  else if (mDrvdTy.find("SDev", 0) == 0) {
    expr = new Metric::StdDevIncr(0, 0, mSrc->id());
    doDispPercent = false;
  }
  else if (mDrvdTy.find("CfVar", 0) == 0) {
    expr = new Metric::CoefVarIncr(0, 0, mSrc->id());
    doDispPercent = false;
  }
  else if (mDrvdTy.find("%CfVar", 0) == 0) {
    expr = new Metric::RStdDevIncr(0, 0, mSrc->id());
    isPercent = true;
  }
  else if (mDrvdTy.find("Min", 0) == 0) {
    expr = new Metric::MinIncr(0, mSrc->id());
    doDispPercent = false;
  }
  else if (mDrvdTy.find("Max", 0) == 0) {
    expr = new Metric::MaxIncr(0, mSrc->id());
    doDispPercent = false;
  }
  else if (mDrvdTy.find("Sum", 0) == 0) {
    expr = new Metric::SumIncr(0, mSrc->id());
  }
  else {
    DIAG_Die(DIAG_UnexpectedInput);
  }
  
  string mNmFmt = mSrc->nameToFmt();
  string mNmBase = mSrc->nameBase() + "-" + mDrvdTy;
  const string& mDesc = mSrc->description();

  DerivedIncrDesc* m =
    new DerivedIncrDesc(mNmFmt, mDesc, expr, true/*isVisible*/,
			true/*isSortKey*/, doDispPercent, isPercent);
  m->nameBase(mNmBase);
  insert(m);
  expr->accumId(m->id());

  if (expr->hasAccum2()) {
    string m2NmBase = mNmBase + "-accum2";
    DerivedIncrDesc* m2 =
      new DerivedIncrDesc(mNmFmt, mDesc, NULL/*expr*/, false/*isVisible*/,
			  false/*isSortKey*/, false/*doDispPercent*/,
			  false/*isPercent*/);
    m2->nameBase(m2NmBase);
    insert(m2);
    expr->accum2Id(m2->id());
  }

  if (expr->hasNumSrcVar()) {
    string mSrcNmBase = mNmBase + "-num-src";
    Metric::NumSourceIncr* mSrcExpr = new Metric::NumSourceIncr(0, mSrc->id());
    DerivedIncrDesc* mSrc =
      new DerivedIncrDesc(mNmFmt, mDesc, mSrcExpr, false/*isVisible*/,
			  false/*isSortKey*/, false/*doDispPercent*/,
			  false/*isPercent*/);
    mSrc->nameBase(mSrcNmBase);
    insert(mSrc);
    mSrcExpr->accumId(mSrc->id());

    expr->numSrcVarId(mSrc->id());
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

uint
Mgr::findGroup(const Mgr& y) const
{
  const Mgr* x = this;

  // N.B.: Cases like the following are handled by the fact that
  // lookups in x are by unique name.
  //   x: [a b c d] [a b e f]
  //   y: [a b e f]

  bool found = true; // optimistic
  std::vector<uint> metricMap(y.size());
  
  for (uint y_i = 0; y_i < y.size(); ++y_i) {
    const Metric::ADesc* y_m = y.metric(y_i);
    string mNm = y_m->name();
    const Metric::ADesc* x_m = x->metric(mNm);
    
    if (!x_m || (y_i > 0 && x_m->id() != (metricMap[y_i - 1] + 1))) {
      found = false;
      break;
    }
    
    metricMap[y_i] = x_m->id();
  }

  return (found && !metricMap.empty()) ? metricMap[0] : Mgr::npos;
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


//****************************************************************************

string
Mgr::toString(const char* pre) const
{
  std::ostringstream os;
  dump(os, pre);
  return os.str();
}


std::ostream&
Mgr::dump(std::ostream& os, const char* pre) const
{
  os << "[ metric table:" << std::endl;
  for (uint i = 0; i < m_metrics.size(); i++) {
    os << "  " << m_metrics[i]->toString() << std::endl;
  }
  os << "]" << std::endl;

  os << "[ unique-name-to-metric:" << std::endl;
  for (StringToADescMap::const_iterator it = m_uniqnmToMetricMap.begin();
       it !=  m_uniqnmToMetricMap.end(); ++it) {
    const string& nm = it->first;
    Metric::ADesc* m = it->second;
    os << "  " << nm << " -> " << m->toString() << std::endl;
  }
  os << "]" << std::endl;

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
  DIAG_Assert(ret.second, "Found duplicate entry; should be unique name!");
  
  // 3. profile file name to Metric::SampledDesc table
  Metric::SampledDesc* mSmpl = dynamic_cast<Metric::SampledDesc*>(m);
  if (mSmpl) {
    const string& fnm = mSmpl->profileName();
    StringToADescVecMap::iterator it = m_fnameToFMetricMap.find(fnm);
    if (it != m_fnameToFMetricMap.end()) {
      Metric::ADescVec& mvec = it->second;
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
