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

Mgr::Mgr()
{
} 


Mgr::~Mgr()
{
  for (uint i = 0; i < m_metrics.size(); ++i) {
    delete m_metrics[i];
  }
} 

//----------------------------------------------------------------------------

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
      const Metric::SampledDesc& m_raw = *mdescs[j];

      bool isSortKey = (empty());
      
      Metric::SampledDesc* m = new Metric::SampledDesc(m_raw);
      m->isSortKey(isSortKey);
      m->doDispPercent(doDispPercent);
      m->isUnitsEvents(isUnitsEvents);

      insert(m);
    }
  }
}


void 
Mgr::makeSummaryMetrics()
{
  StringToADescVecMap::iterator it = m_nuniqnmToMetricMap.begin();
  for ( ; it != m_nuniqnmToMetricMap.end(); ++it) {
    const string& m_nm = it->first;
    Metric::ADescVec& mvec = it->second;
    if (mvec.size() > 1) {
      string mean_nm = "Mean-" + m_nm;
      //string rsd_nm = "RStdDev-" + m_nm;
      string cv_nm = "CoefVar-" + m_nm;
      string min_nm = "Min-" + m_nm;
      string max_nm = "Max-" + m_nm;
      string sum_nm = "Sum-" + m_nm;
      makeSummaryMetric(mean_nm, mvec);
      //makeSummaryMetric(rsd_nm, mvec);
      makeSummaryMetric(cv_nm, mvec);
      makeSummaryMetric(min_nm, mvec);
      makeSummaryMetric(max_nm, mvec);
      makeSummaryMetric(sum_nm, mvec);
    }
  }
}


void
Mgr::makeSummaryMetric(const string& m_nm, const Metric::ADescVec& m_opands)
{
  Metric::AExpr** opands = new Metric::AExpr*[m_opands.size()];
  for (uint i = 0; i < m_opands.size(); ++i) {
    Metric::ADesc* m = m_opands[i];
    opands[i] = new Metric::Var(m->name(), m->id());
  }

  bool isVisible = true;
  bool doDispPercent = true;
  bool isPercent = false;

  // This is a a cheesy way of creating the metrics, but it is good
  // enough for now.  Perhaps, this can be pushed into a metric parser
  // as mathxml is retired.
  Metric::AExpr* expr = NULL;
  if (m_nm.find("Mean", 0) == 0) {
    expr = new Metric::Mean(opands, m_opands.size());
    doDispPercent = false;
  }
  else if (m_nm.find("StdDev", 0) == 0) {
    expr = new Metric::StdDev(opands, m_opands.size());
    doDispPercent = false;
  }
  else if (m_nm.find("RStdDev", 0) == 0) {
    expr = new Metric::RStdDev(opands, m_opands.size());
    isPercent = true;
  }
  else if (m_nm.find("CoefVar", 0) == 0) {
    expr = new Metric::CoefVar(opands, m_opands.size());
    doDispPercent = false;
  }
  else if (m_nm.find("Min", 0) == 0) {
    expr = new Metric::Min(opands, m_opands.size());
    doDispPercent = false;
  }
  else if (m_nm.find("Max", 0) == 0) {
    expr = new Metric::Max(opands, m_opands.size());
    doDispPercent = false;
  }
  else if (m_nm.find("Sum", 0) == 0) {
    expr = new Metric::Plus(opands, m_opands.size());
  }
  else {
    DIAG_Die(DIAG_UnexpectedInput);
  }
  
  insert(new DerivedDesc(m_nm, m_nm, expr, isVisible, true/*isSortKey*/,
			 doDispPercent, isPercent));
}


//----------------------------------------------------------------------------

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
  Metric::ADesc* m_sortby = NULL;
  for (uint i = 0; i < m_metrics.size(); ++i) {
    Metric::ADesc* m = m_metrics[i]; 
    if (m->isSortKey()) {
      m_sortby = m;
      break;
    }
  }
  return m_sortby;
}


bool
Mgr::hasDerived() const
{
  for (uint i = 0; i < m_metrics.size(); ++i) {
    Metric::ADesc* m = m_metrics[i]; 
    if (typeid(*m) == typeid(Prof::Metric::DerivedDesc)) {
      return true;
    }
  }
  return false;
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
  Metric::SampledDesc* m_fm = dynamic_cast<Metric::SampledDesc*>(m);
  if (m_fm) {
    const string& fnm = m_fm->profileName();
    StringToADescVecMap::iterator it = m_fnameToFMetricMap.find(fnm);
    if (it != m_fnameToFMetricMap.end()) {
      Metric::ADescVec& mvec = it->second;
      mvec.push_back(m_fm);
    }
    else {
      m_fnameToFMetricMap.insert(make_pair(fnm, Metric::ADescVec(1, m_fm)));
    }
  }

  return isChanged;
}

//****************************************************************************

} // namespace Metric

} // namespace Prof
