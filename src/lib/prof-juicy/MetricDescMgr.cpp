// -*-Mode: C++;-*-
// $Id$

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

//************************ System Include Files ******************************

#include <iostream>

#include <string>
using std::string;

#include <map>
using std::map;

#include <vector>
using std::vector;

//************************* User Include Files *******************************

#include "MetricDescMgr.hpp"

#include <lib/prof-juicy/FlatProfileReader.hpp>

#include <lib/support/diagnostics.h>
#include <lib/support/StrUtil.hpp>

//************************ Forward Declarations ******************************


//****************************************************************************

Prof::MetricDescMgr::MetricDescMgr()
{
} 


Prof::MetricDescMgr::~MetricDescMgr()
{
  for (uint i = 0; i < m_metrics.size(); ++i) {
    delete m_metrics[i];
  }
} 

//----------------------------------------------------------------------------

void 
Prof::MetricDescMgr::makeRawMetrics(const std::vector<std::string>& profileFiles,
				    bool isunit_ev)
{
  // ------------------------------------------------------------
  // Create a FilePerfMetric for each event within each profile
  // ------------------------------------------------------------
  for (uint i = 0; i < profileFiles.size(); ++i) {
    const string& proffnm = profileFiles[i];

    Prof::Flat::Profile prof;
    try {
      prof.open(proffnm.c_str());
    }
    catch (...) {
      DIAG_EMsg("While reading '" << proffnm << "'");
      throw;
    }

    const Prof::SampledMetricDescVec& mdescs = prof.mdescs();
    for (uint j = 0; j < mdescs.size(); ++j) {
      const Prof::SampledMetricDesc& m_raw = *mdescs[j];
      
      string nativeNm = StrUtil::toStr(j);
      bool sortby = empty();
      FilePerfMetric* m = new FilePerfMetric(m_raw.name(), nativeNm,
					     m_raw.name(),
					     true/*display*/, true/*percent*/,
					     sortby, proffnm, string("HPCRUN"),
					     isunit_ev);
      m->rawdesc(m_raw);
      insert(m);
    }
  }

  // ------------------------------------------------------------
  // Create computed metrics
  // ------------------------------------------------------------
}


void 
Prof::MetricDescMgr::makeSummaryMetrics()
{
  StringPerfMetricVecMap::iterator it = m_nuniqnmToMetricMap.begin();
  for ( ; it != m_nuniqnmToMetricMap.end(); ++it) {
    const string& m_nm = it->first;
    PerfMetricVec& mvec = it->second;
    if (mvec.size() > 1) {
      string sum_nm = "SUM-" + m_nm;
      string min_nm = "MIN-" + m_nm;
      string max_nm = "MAX-" + m_nm;
      makeSummaryMetric(sum_nm, mvec);
      makeSummaryMetric(min_nm, mvec);
      makeSummaryMetric(max_nm, mvec);
    }
  }
}


void
Prof::MetricDescMgr::makeSummaryMetric(const string& m_nm,
				       const PerfMetricVec& m_opands)
{
  EvalNode** opands = new EvalNode*[m_opands.size()];
  for (uint i = 0; i < m_opands.size(); ++i) {
    PerfMetric* m = m_opands[i];
    opands[i] = new Var(m->Name(), m->Index());
  }

  bool doDisplay = true;
  bool doPercent = true;

  EvalNode* expr = NULL;
  if (m_nm.find("SUM", 0) == 0) {
    expr = new Plus(opands, m_opands.size());
  }
  else if (m_nm.find("MIN", 0) == 0) {
    expr = new Min(opands, m_opands.size());
    doPercent = false;
  }
  else if (m_nm.find("MAX", 0) == 0) {
    expr = new Max(opands, m_opands.size());
    doPercent = false;
  }
  else {
    DIAG_Die(DIAG_UnexpectedInput);
  }
  
  insert(new ComputedPerfMetric(m_nm, m_nm, doDisplay/*display*/, 
				doPercent/*percent*/, true/*sortby*/,
				false/*propagateComputed*/, expr));
}


//----------------------------------------------------------------------------

bool 
Prof::MetricDescMgr::insert(PerfMetric* m)
{ 
  bool ans = false;
  
  // 1. metric table
  uint id = m_metrics.size();
  m_metrics.push_back(m);
  m->Index(id);

  // 2. metric name to PerfMetricVec table
  const string& nm = m->Name();
  StringPerfMetricVecMap::iterator it = m_nuniqnmToMetricMap.find(nm);
  if (it != m_nuniqnmToMetricMap.end()) {
    PerfMetricVec& mvec = it->second;

    // ensure uniqueness
    int qualifier = mvec.size();
    string nm_new = nm + "-" + StrUtil::toStr(qualifier);
    
    m->Name(nm_new);
    m->DisplayInfo().Name(nm_new);
    ans = true; 

    mvec.push_back(m);
  }
  else {
    m_nuniqnmToMetricMap.insert(make_pair(nm, PerfMetricVec(1, m)));
  }

  // 3. unique name to PerfMetric table
  std::pair<StringPerfMetricMap::iterator, bool> ret = 
    m_uniqnmToMetricMap.insert(make_pair(nm, m));
  DIAG_Assert(ret.second, "Found duplicate entry; should be unique name!");
  
  // 4. profile file name to FilePerfMetric table
  FilePerfMetric* m_fm = dynamic_cast<FilePerfMetric*>(m);
  if (m_fm) {
    const string& fnm = m_fm->FileName();
    StringPerfMetricVecMap::iterator it = m_fnameToFMetricMap.find(fnm);
    if (it != m_fnameToFMetricMap.end()) {
      PerfMetricVec& mvec = it->second;
      mvec.push_back(m_fm);
    }
    else {
      m_fnameToFMetricMap.insert(make_pair(fnm, PerfMetricVec(1, m_fm)));
    }
  }

  return ans;
}


PerfMetric*
Prof::MetricDescMgr::findSortBy() const
{
  PerfMetric* m_sortby = NULL;
  for (uint i = 0; i < m_metrics.size(); ++i) {
    PerfMetric* m = m_metrics[i]; 
    if (m->SortBy()) {
      m_sortby = m;
      break;
    }
  }
  return m_sortby;
}


bool 
Prof::MetricDescMgr::hasDerived() const
{
  for (uint i = 0; i < m_metrics.size(); ++i) {
    PerfMetric* m = m_metrics[i]; 
    ComputedPerfMetric* mm = dynamic_cast<ComputedPerfMetric*>(m);
    if (mm) {
      return true;
    }
  }
  return false;
}


string
Prof::MetricDescMgr::toString(const char* pre) const
{
  std::ostringstream os;
  dump(os, pre);
  return os.str();
}


void
Prof::MetricDescMgr::dump(std::ostream& o, const char* pre) const
{
  for (uint i = 0; i < m_metrics.size(); i++) {
    o << m_metrics[i]->toString() + "\n";
  }
}


void
Prof::MetricDescMgr::ddump() const
{
  dump(std::cerr);
}
