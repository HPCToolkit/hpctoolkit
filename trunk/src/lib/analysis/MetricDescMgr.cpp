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

Analysis::MetricDescMgr::MetricDescMgr()
{
} 


Analysis::MetricDescMgr::~MetricDescMgr()
{
} 


string 
Analysis::MetricDescMgr::makeUniqueName(const std::string& nm)
{
  StringToIntMap::iterator it = m_metricDisambiguationTbl.find(nm);
  if (it != m_metricDisambiguationTbl.end()) {
    int& qualifier = it->second;
    qualifier++;
    string nm_new = nm + "-" + StrUtil::toStr(qualifier);
    return nm_new;
  }
  else {
    m_metricDisambiguationTbl.insert(make_pair(nm, 0));
    return nm;
  }
}


void 
Analysis::MetricDescMgr::makePerfMetricDescs(std::vector<std::string>& profileFiles)
{
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

    // For each metric: compute a canonical name and create a metric desc.
    const Prof::SampledMetricDescVec& mdescs = prof.mdescs();
    for (uint j = 0; j < mdescs.size(); ++j) {
      const Prof::SampledMetricDesc& m = *mdescs[j];
      
      string metricNm = makeUniqueName(m.name());
      string nativeNm = StrUtil::toStr(j);
      bool metricDoSortBy = empty();
      
      insert(new FilePerfMetric(metricNm, nativeNm, metricNm, 
				true /*display*/, true /*percent*/, 
				metricDoSortBy, proffnm, 
				string("HPCRUN")));
    }
  }
}


string
Analysis::MetricDescMgr::toString(const char* pre) const
{
  std::ostringstream os;
  dump(os, pre);
  return os.str();
}


void
Analysis::MetricDescMgr::dump(std::ostream& o, const char* pre) const
{
  for (uint i = 0; i < m_metrics.size(); i++) {
    o << m_metrics[i]->ToString() + "\n";
  }
}


void
Analysis::MetricDescMgr::ddump() const
{
  dump(std::cerr);
}
