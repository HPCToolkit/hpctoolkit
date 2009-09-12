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

#ifndef prof_juicy_Prof_Metric_Mgr_hpp
#define prof_juicy_Prof_Metric_Mgr_hpp 

//************************ System Include Files ******************************

#include <iostream>
#include <string>
#include <list>
#include <vector>
#include <map>


//************************* User Include Files *******************************

#include <include/uint.h>

#include "Metric-OLD.hpp"

#include <lib/support/Unique.hpp>

//************************ Forward Declarations ******************************

namespace Prof {

namespace Metric {

//****************************************************************************

class Mgr : public Unique { // non copyable
public:
  typedef std::vector<PerfMetric*> PerfMetricVec;
  typedef std::map<std::string, PerfMetric*> StringPerfMetricMap;
  typedef std::map<std::string, PerfMetricVec> StringPerfMetricVecMap;

public:
  Mgr();
  ~Mgr();

  void 
  makeRawMetrics(const std::vector<std::string>& profileFiles,
		 bool isunit_ev = true,
		 bool ispercent = true);

  void 
  makeRawMetrics(const std::string& profileFile,
		 bool isunit_ev = true,
		 bool ispercent = true)
  {
    std::vector<std::string> vec(1, profileFile);
    makeRawMetrics(vec, isunit_ev, ispercent);
  }

  void
  makeSummaryMetrics();


  // ------------------------------------------------------------
  // The metric table
  // 
  // INVARIANTS:
  // - All 'raw' metrics are independent of each other
  // - A metric's id is always within [0 size())
  // - A derived metric is dependent only upon 'prior' metrics,
  //   i.e. metrics with ids strictly less than its own.
  //   
  // ------------------------------------------------------------
  PerfMetric*
  metric(int i)
  {
    return m_metrics[i];
  }

  const PerfMetric*
  metric(int i) const
  {
    return m_metrics[i];
  }

  PerfMetric*
  metric(const std::string& uniqNm)
  {
    StringPerfMetricMap::const_iterator it = m_uniqnmToMetricMap.find(uniqNm);
    return (it != m_uniqnmToMetricMap.end()) ? it->second : NULL;
  }

  const PerfMetric*
  metric(const std::string& uniqNm) const
  {
    StringPerfMetricMap::const_iterator it = m_uniqnmToMetricMap.find(uniqNm);
    return (it != m_uniqnmToMetricMap.end()) ? it->second : NULL;
  }

  uint
  size() const
  { return m_metrics.size(); }

  bool
  empty() const
  { return m_metrics.empty(); }

  // Given m, insert m into the tables, ensuring it has a unique name
  // by qualifying it if necessary.  Returns true if the name was
  // modified, false otherwise.
  // NOTE: Assumes ownership of 'm'
  bool 
  insert(PerfMetric* m);

  // Return the (first) metric this has the sort-by attribute set
  PerfMetric*
  findSortBy() const;

  bool
  hasDerived() const;
  
  // ------------------------------------------------------------
  // helper tables
  // ------------------------------------------------------------

  const StringPerfMetricVecMap&
  fnameToFMetricMap() const
  { return m_fnameToFMetricMap; }


  // ------------------------------------------------------------
  // 
  // ------------------------------------------------------------
  std::string
  toString(const char* pre = "") const;

  void
  dump(std::ostream& o = std::cerr, const char* pre = "") const;

  void
  ddump() const;

public:

  typedef std::list<FilePerfMetric*> MetricList_t;

private:
  std::string
  makeUniqueName(const std::string& nm);

  void
  makeSummaryMetric(const std::string& m_nm, const PerfMetricVec& m_opands);

  
private:
  // the metric table
  PerfMetricVec m_metrics;

  // non-unique-metric name to PerfMetricVec table (i.e., name excludes
  // qualifications added by insert()
  StringPerfMetricVecMap m_nuniqnmToMetricMap;

  // unique-metric name to PerfMetricVec table
  StringPerfMetricMap m_uniqnmToMetricMap;

  // profile file name to FilePerfMetric table
  StringPerfMetricVecMap m_fnameToFMetricMap;
};

//****************************************************************************

} // namespace Metric

} // namespace Prof


//****************************************************************************

#endif // prof_juicy_Prof_Metric_Mgr_hpp
