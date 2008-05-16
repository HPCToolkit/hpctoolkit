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

#ifndef Analysis_MetricDescMgr_hpp
#define Analysis_MetricDescMgr_hpp 

//************************ System Include Files ******************************

#include <iostream>
#include <string>
#include <list>
#include <vector>
#include <map>


//************************* User Include Files *******************************

#include <include/general.h>

#include "DerivedPerfMetrics.hpp"

#include <lib/support/Unique.hpp>

//************************ Forward Declarations ******************************

namespace Analysis {

//****************************************************************************

class MetricDescMgr : public Unique { // non copyable
public: 
  MetricDescMgr();
  ~MetricDescMgr(); 

  void makePerfMetricDescs(std::vector<std::string>& profileFiles);

  // ------------------------------------------------------------
  // 
  // ------------------------------------------------------------
  PerfMetric* metric(int i) const { return m_metrics[i]; }
  void insert(PerfMetric* m) { m_metrics.push_back(m); }
  uint size() const { return m_metrics.size(); }
  bool empty() const { return m_metrics.empty(); }

  // ------------------------------------------------------------
  // 
  // ------------------------------------------------------------
  std::string 
  toString(const char* pre = "") const;

  void 
  dump(std::ostream& o = std::cerr, const char* pre = "") const;

  void 
  ddump() const;


  typedef std::list<FilePerfMetric*> MetricList_t;
private:
  typedef std::map<string, int> StringToIntMap;

  std::string makeUniqueName(const std::string& nm);
  
private:
  std::vector<PerfMetric*> m_metrics;

  StringToIntMap m_metricDisambiguationTbl;
};

//****************************************************************************

} // namespace Analysis

//****************************************************************************

#endif // Analysis_MetricDescMgr_hpp
