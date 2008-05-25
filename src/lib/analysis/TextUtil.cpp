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

//***************************************************************************
//
// File:
//   $Source$
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
using std::ostream;

#include <iomanip>

#include <string>
using std::string;

#include <vector>
using std::vector;

#include <algorithm>

#include <cmath> // pow

//*************************** User Include Files ****************************

#include "TextUtil.hpp"

#include <lib/support/diagnostics.h>

//*************************** Forward Declarations ***************************

//****************************************************************************

namespace Analysis {

namespace TextUtil {

ColumnFormatter::ColumnFormatter(const Prof::MetricDescMgr& metricMgr,
				 ostream& os, int num_digits)
  : m_mMgr(metricMgr),
    m_os(os),
    m_num_digits(num_digits),
    m_metricAnnotationWidthTot(0)
{ 
  m_metricAnnotationWidth.resize(m_mMgr.size(), 0);
  m_scientificFormatThreshold.resize(m_mMgr.size(), 0.0);

  for (uint mId = 0; mId < m_mMgr.size(); ++mId) {
    const PerfMetric* m = m_mMgr.metric(mId);
    if (!m->Display()) {
      continue;
    }
    
    // Compute annotation width
    if (m->Percent()) {
      if (m_num_digits >= 1) {
	// xxx.(yy)% or x.xE-yy% (for small numbers)
	m_metricAnnotationWidth[mId] = std::max(8, 5 + m_num_digits);
	m_scientificFormatThreshold[mId] = 
	  std::pow((double)10, -(double)m_num_digits);
      }
      else {
	// xxx%
	m_metricAnnotationWidth[mId] = 4;
      }
    }
    else {
      // x.xE+yy (for large numbers)
      m_metricAnnotationWidth[mId] = 7;
      
      // for floating point numbers over the scientificFormatThreshold
      // printed in scientific format.
      m_scientificFormatThreshold[mId] = 
	std::pow((double)10, (double)m_metricAnnotationWidth[mId]);
    }

    m_metricAnnotationWidthTot += m_metricAnnotationWidth[mId] + 1;
  }

  m_os.setf(std::ios_base::fmtflags(0), std::ios_base::floatfield);
  m_os << std::right << std::noshowpos;
  m_os << std::showbase;
}


void
ColumnFormatter::genCol(uint mId, uint64_t metricVal, uint64_t metricTot)
{
  const PerfMetric* m = m_mMgr.metric(mId);

  if (!isDisplayed(mId)) {
    return;
  }
  
  if (m->Percent()) {
    double val = 0.0;
    if (metricTot != 0) {
      val = ((double)metricVal / (double)metricTot) * 100;
    }
    
    m_os << std::showpoint << std::setw(m_metricAnnotationWidth[mId] - 1);
    if (val != 0.0 && val < m_scientificFormatThreshold[mId]) {
      m_os << std::scientific 
	   << std::setprecision(m_metricAnnotationWidth[mId] - 7);
    }
    else {
      //m_os.unsetf(ios_base::scientific);
      m_os << std::fixed
	   << std::setprecision(m_num_digits);
    }
    m_os << std::setfill(' ') << val << "%";
  }
  else {
    m_os << std::scientific
	 << std::setprecision(m_metricAnnotationWidth[mId] - 6)
	 << std::setw(m_metricAnnotationWidth[mId]);
    if ((double)metricVal >= m_scientificFormatThreshold[mId]) {
      m_os << (double)metricVal;
    }
    else {
      m_os << std::setfill(' ') << metricVal;
    }
  }
  m_os << " ";
}


//****************************************************************************

#if 0
void
writeMetricSummary(std::ostream& os, 
		   const vector<const Prof::Flat::EventData*>& metricDescs,
		   const vector<uint64_t>& metricVal,
		   const vector<uint64_t>* metricTot)
#endif


} // namespace TextUtil

} // namespace Analysis
