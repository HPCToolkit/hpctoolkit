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

#ifndef prof_Prof_Metric_IData_hpp 
#define prof_Prof_Metric_IData_hpp

//************************* System Include Files ****************************

#include <iostream>

#include <string>
#include <vector>

#include <typeinfo>
#include <algorithm>

#include <climits>
 
//*************************** User Include Files ****************************

#include <include/uint.h>

#include <lib/support/diagnostics.h>


//*************************** Forward Declarations **************************


//***************************************************************************

namespace Prof {
namespace Metric {

//***************************************************************************
// IData
//
// Interface/Mixin for metric data
//
// Optimized for the two expected common cases:
//   1. no metrics (hpcstruct's using Prof::Struct::Tree)
//   2. a known number of metrics (which may then be expanded)
//***************************************************************************

class IData {
public:
  
  typedef std::vector<double> MetricVec;

public:
  // --------------------------------------------------------
  // Create/Destroy
  // --------------------------------------------------------
  IData(size_t size = 0)
    : m_metrics(NULL)
  {
    if (size != 0) {
      ensureMetricsSize(size);
    }
  }

  virtual ~IData()
  {
    delete m_metrics;
  }
  
  IData(const IData& x)
    : m_metrics(NULL)
  {
    if (x.m_metrics) {
      m_metrics = new MetricVec(*(x.m_metrics));
    }
  }
  
  IData&
  operator=(const IData& x)
  {
    if (this != &x) {
      clearMetrics();
      if (x.m_metrics) {
	m_metrics = new MetricVec(*(x.m_metrics));
      }
    }
    return *this;
  }

  // --------------------------------------------------------
  // Metrics
  // --------------------------------------------------------

  static const uint npos = UINT_MAX;

  bool
  hasMetrics(uint mBegId = Metric::IData::npos,
	     uint mEndId = Metric::IData::npos) const
  {
    if (mBegId == IData::npos) {
      mBegId = 0;
    }
    mEndId = std::min(numMetrics(), mEndId);

    for (uint i = mBegId; i < mEndId; ++i) {
      if (hasMetric(i)) {
	return true;
      }
    }
    return false;
  }

  bool
  hasMetric(size_t mId) const
  { return ((*m_metrics)[mId] != 0.0); }

  bool
  hasMetricSlow(size_t mId) const
  { return (m_metrics && mId < m_metrics->size() && hasMetric(mId)); }


  double
  metric(size_t mId) const
  { return (*m_metrics)[mId]; }

  double&
  metric(size_t mId)
  { return (*m_metrics)[mId]; }


  double
  demandMetric(size_t mId, size_t size = 0) const
  {
    size_t sz = std::max(size, mId+1);
    ensureMetricsSize(sz);
    return metric(mId);
  }

  double&
  demandMetric(size_t mId, size_t size = 0)
  {
    size_t sz = std::max(size, mId+1);
    ensureMetricsSize(sz);
    return metric(mId);
  }


  // zeroMetrics: takes bounds of the form [mBegId, mEndId)
  // N.B.: does not have demandZeroMetrics() semantics
  void
  zeroMetrics(uint mBegId, uint mEndId)
  {
    for (uint i = mBegId; i < mEndId; ++i) {
      metric(i) = 0.0;
    }
  }


  void
  clearMetrics()
  {
    delete m_metrics;
    m_metrics = NULL;
  }


  // --------------------------------------------------------
  // 
  // --------------------------------------------------------

  // ensureMetricsSize: ensures a vector of the requested size exists
  void
  ensureMetricsSize(size_t size) const
  {
    if (!m_metrics) {
      m_metrics = new MetricVec(size, 0.0 /*value*/);
    }
    else if (size > m_metrics->size()) {
      m_metrics->resize(size, 0.0 /*value*/); // inserts at end
    }
  }

  void
  insertMetricsBefore(size_t numMetrics) 
  {
    if (numMetrics > 0 && !m_metrics) {
      m_metrics = new MetricVec();
    }
    for (uint i = 0; i < numMetrics; ++i) {
      m_metrics->insert(m_metrics->begin(), 0.0);
    }
  }
  
  uint
  numMetrics() const
  { return (m_metrics) ? m_metrics->size() : 0; }


  // --------------------------------------------------------
  // 
  // --------------------------------------------------------
  
  std::string
  toStringMetrics(int oFlags = 0, const char* pfx = "") const;


  // [mBegId, mEndId)
  std::ostream& 
  writeMetricsXML(std::ostream& os,
		  uint mBegId = Metric::IData::npos,
		  uint mEndId = Metric::IData::npos,
		  int oFlags = 0, const char* pfx = "") const;


  std::ostream&
  dumpMetrics(std::ostream& os = std::cerr, int oFlags = 0,
	      const char* pfx = "") const;

  void
  ddumpMetrics() const;

  
private:
  mutable MetricVec* m_metrics; // 'mutable' for ensureMetricsSize()
};

//***************************************************************************

} // namespace Metric
} // namespace Prof


#endif /* prof_Prof_Metric_IData_hpp */
