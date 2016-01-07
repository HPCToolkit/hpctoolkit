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

#ifndef prof_Prof_Metric_Mgr_hpp
#define prof_Prof_Metric_Mgr_hpp

//************************ System Include Files ******************************

#include <iostream>
#include <string>
#include <list>
#include <vector>
#include <map>

#include <climits>

//************************* User Include Files *******************************

#include <include/uint.h>

#include "Metric-ADesc.hpp"

#include <lib/support/Unique.hpp>

//************************ Forward Declarations ******************************

namespace Prof {

namespace Metric {

//****************************************************************************

class Mgr
  : public Unique // non copyable
{
public:
  typedef std::map<std::string, Metric::ADesc*> StringToADescMap;
  typedef std::map<std::string, Metric::ADescVec> StringToADescVecMap;

public:
  Mgr();
  ~Mgr();

  // ------------------------------------------------------------
  // make raw metrics (N.B.: currently only for flat profiles)
  // ------------------------------------------------------------

  void
  makeRawMetrics(const std::vector<std::string>& profileFiles,
		 bool isUnitsEvents = true,
		 bool doDispPercent = true);

  void
  makeRawMetrics(const std::string& profileFile,
		 bool isUnitsEvents = true,
		 bool doDispPercent = true)
  {
    std::vector<std::string> vec(1, profileFile);
    makeRawMetrics(vec, isUnitsEvents, doDispPercent);
  }


  // ------------------------------------------------------------
  // make summary metrics: [srcBegId, srcEndId)
  // ------------------------------------------------------------

  uint
  makeSummaryMetrics(bool needAllStats, bool needMultiOccurance,
		     uint srcBegId = Mgr::npos, uint srcEndId = Mgr::npos);

  uint
  makeSummaryMetricsIncr(bool needAllStats,
			 uint srcBegId = Mgr::npos,
                         uint srcEndId = Mgr::npos);


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
  Metric::ADesc*
  metric(uint i)
  { return m_metrics[i]; }

  const Metric::ADesc*
  metric(uint i) const
  { return m_metrics[i]; }

  Metric::ADesc*
  metric(const std::string& uniqNm)
  {
    StringToADescMap::const_iterator it = m_uniqnmToMetricMap.find(uniqNm);
    return (it != m_uniqnmToMetricMap.end()) ? it->second : NULL;
  }

  const Metric::ADesc*
  metric(const std::string& uniqNm) const
  {
    StringToADescMap::const_iterator it = m_uniqnmToMetricMap.find(uniqNm);
    return (it != m_uniqnmToMetricMap.end()) ? it->second : NULL;
  }

  uint
  size() const
  { return m_metrics.size(); }

  bool
  empty() const
  { return m_metrics.empty(); }


  // ------------------------------------------------------------
  // insert
  // ------------------------------------------------------------

  // Given m, insert m into the tables, ensuring it has a unique name
  // by qualifying it if necessary.  Returns true if the name was
  // modified, false otherwise.
  // NOTE: Assumes ownership of 'm'
  bool
  insert(Metric::ADesc* m);

  // Given m, insert m into the tables if the metric name does not
  // exist.  Returns true if inserted (assuming ownership) and false
  // otherwise.
  bool
  insertIf(Metric::ADesc* m);

  // ------------------------------------------------------------
  // 
  // ------------------------------------------------------------

  // Return the (first) metric this has the sort-by attribute set
  Metric::ADesc*
  findSortKey() const;

  // Return the last metric that is visible
  Metric::ADesc*
  findFirstVisible() const;

  // Return the last metric that is visible
  Metric::ADesc*
  findLastVisible() const;

  bool
  hasDerived() const;


  // ------------------------------------------------------------
  // 
  // ------------------------------------------------------------

  static const uint npos = UINT_MAX;

  // findGroup: finds the group of metrics in 'x' = 'this' that
  //   correspond to those in 'y' (i.e., a mapping between 'y' and
  //   'x').  Returns the first index of the group if found; otherwise
  //   Mgr::npos.
  uint
  findGroup(const Mgr& y_mMgr) const;

  
  // ------------------------------------------------------------
  // helper maps/tables
  // ------------------------------------------------------------

  const StringToADescVecMap&
  fnameToFMetricMap() const
  { return m_fnameToFMetricMap; }

  void
  recomputeMaps();

  void
  computePartners();


  // ------------------------------------------------------------
  // 
  // ------------------------------------------------------------

  void
  zeroDBInfo() const;
  
  // ------------------------------------------------------------
  // 
  // ------------------------------------------------------------

  std::string
  toString(const char* pfx = "") const;

  std::ostream&
  dump(std::ostream& os = std::cerr, const char* pfx = "") const;

  void
  ddump() const;

public:

private:
  bool
  insertInMapsAndMakeUniqueName(Metric::ADesc* m);

  Metric::DerivedDesc*
  makeSummaryMetric(const std::string mDrvdTy, const Metric::ADesc* mSrc,
		    const Metric::ADescVec& mOpands);

  Metric::DerivedIncrDesc*
  makeSummaryMetricIncr(const std::string mDrvdTy, const Metric::ADesc* mSrc);

  
private:
  // the metric table
  Metric::ADescVec m_metrics;

  // non-unique-metric name to Metric::ADescVec table (i.e., name excludes
  // qualifications added by insert())
  StringToADescVecMap m_nuniqnmToMetricMap;

  // unique-metric name to Metric::ADescVec table
  StringToADescMap m_uniqnmToMetricMap;

  // profile file name to Metric::SampledDesc table
  StringToADescVecMap m_fnameToFMetricMap;
};

//****************************************************************************

} // namespace Metric

} // namespace Prof


//****************************************************************************

#endif // prof_Prof_Metric_Mgr_hpp
