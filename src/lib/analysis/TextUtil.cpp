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

//************************* System Include Files ****************************

#include <iostream>
using std::ostream;

#include <iomanip>

#include <string>
using std::string;

#include <vector>
using std::vector;

#include <algorithm>

#include <typeinfo>

#include <cmath> // pow

//*************************** User Include Files ****************************

#include "TextUtil.hpp"

#include <lib/support/diagnostics.h>

//*************************** Forward Declarations ***************************

//****************************************************************************

namespace Analysis {

namespace TextUtil {

ColumnFormatter::ColumnFormatter(const Prof::Metric::Mgr& metricMgr,
				 ostream& os, 
				 int numDecPct, int numDecVal)
  : m_mMgr(metricMgr),
    m_os(os),
    m_numDecPct(numDecPct),
    m_numDecVal(numDecVal),
    m_annotWidthTot(0)
{ 
  m_annotWidth.resize(m_mMgr.size(), 0);
  m_sciFmtLoThrsh_pct.resize(m_mMgr.size(), 0.0);
  m_sciFmtHiThrsh_pct.resize(m_mMgr.size(), 0.0);
  m_sciFmtLoThrsh_val.resize(m_mMgr.size(), 0.0);
  m_sciFmtHiThrsh_val.resize(m_mMgr.size(), 0.0);
  m_dispPercent.resize(m_mMgr.size(), false);
  m_isForceable.resize(m_mMgr.size(), false);

  for (uint mId = 0; mId < m_mMgr.size(); ++mId) {
    const Prof::Metric::ADesc* m = m_mMgr.metric(mId);
    if (!m->isVisible()) {
      continue;
    }
    
    m_isForceable[mId] = (typeid(*m) == typeid(Prof::Metric::SampledDesc)
			  || !m->isPercent());

    // Compute annotation widths. 
    // NOTE: decimal digits shown as 'd' below
    if (m->doDispPercent()) {
      m_dispPercent[mId] = true;
      if (m_numDecPct >= 1) { 
	// xxx.dd% or x.dE-yy% (latter for small values)
	m_annotWidth[mId] = std::max(8, 5 + m_numDecPct);
      }
      else {
	// xxx%
	m_annotWidth[mId] = 4;
      }
    }
    else {
      // x.dd or x.dE+yy (latter for large numbers)
      m_annotWidth[mId] = std::max(7, 2 + m_numDecVal);
    }

    // compute threshholds
    int nNonUnitsPct = (m_numDecPct == 0) ? 1 : m_numDecPct + 2; // . + %
    int nNonUnitsVal = (m_numDecVal == 0) ? 0 : m_numDecVal + 1; // .

    int nUnitsPct = std::max(0, m_annotWidth[mId] - nNonUnitsPct);
    int nUnitsVal = std::max(0, m_annotWidth[mId] - nNonUnitsVal);

    m_sciFmtHiThrsh_pct[mId] = std::pow(10.0, (double)nUnitsPct);
    m_sciFmtHiThrsh_val[mId] = std::pow(10.0, (double)nUnitsVal);

    m_sciFmtLoThrsh_pct[mId] = std::pow(10.0, (double)-m_numDecPct);
    m_sciFmtLoThrsh_val[mId] = std::pow(10.0, (double)-m_numDecVal);

    m_annotWidthTot += m_annotWidth[mId] + 1/*space*/;
  }

  m_os.setf(std::ios_base::fmtflags(0), std::ios_base::floatfield);
  m_os << std::right << std::noshowpos;
  m_os << std::showbase;
}


void
ColumnFormatter::genColHeaderSummary()
{
  m_os << "Metric definitions. column: name (nice-name) [units] {details}:\n";

  uint colId = 1;
  for (uint mId = 0; mId < m_mMgr.size(); ++mId) {
    const Prof::Metric::ADesc* m = m_mMgr.metric(mId);

    m_os << std::fixed << std::setw(4) << std::setfill(' ');
    if (m->isVisible()) {
      m_os << colId;
      colId++;
    }
    else {
      m_os << "-";
    }

    m_os << ": " << m->toString() << std::endl;
  }
}


void
ColumnFormatter::genCol(uint mId, double metricVal, double metricTot,
			ColumnFormatter::Flag flg)
{
  if (!isDisplayed(mId)) {
    return;
  }

  bool dispPercent = m_dispPercent[mId];
  if (flg != Flag_NULL && m_isForceable[mId]) {
    dispPercent = (flg == Flag_ForcePct);
  }

  double val = metricVal;

  if (dispPercent) {
    // convert 'val' to a percent if necessary
    const Prof::Metric::ADesc* m = m_mMgr.metric(mId);
    if (!m->isPercent() && metricTot != 0.0) {
      val = (metricVal / metricTot) * 100.0; // make the percent
    }
    
    m_os << std::showpoint << std::setw(m_annotWidth[mId] - 1);
    if (val != 0.0 &&
	(val < m_sciFmtLoThrsh_pct[mId] || val >= m_sciFmtHiThrsh_pct[mId])) {
      m_os << std::scientific 
	   << std::setprecision(m_annotWidth[mId] - 7); // x.dE-yy%
    }
    else {
      m_os << std::fixed
	   << std::setprecision(m_numDecPct);
    }
    m_os << std::setfill(' ') << val << "%";
  }
  else {
    m_os << std::setw(m_annotWidth[mId]);
    if (val != 0.0 &&
	(val < m_sciFmtLoThrsh_val[mId] || val >= m_sciFmtHiThrsh_val[mId])) {
      m_os << std::scientific
	   << std::setprecision(m_annotWidth[mId] - 6); // x.dE+yy
    }
    else {
      m_os << std::fixed;
      if (m_numDecVal == 0) {
	m_os << std::noshowpoint << std::setprecision(0);
      }
      else {
	m_os << std::setprecision(m_numDecVal);
      }
    }
    m_os << std::setfill(' ') << val;
  }
  m_os << " ";
}


void
ColumnFormatter::genBlankCol(uint mId)
{
  //const PerfMetric* m = m_mMgr.metric(mId);
  if (!isDisplayed(mId)) {
    return;
  }
  m_os << std::setw(m_annotWidth[mId]) << std::setfill(' ') << " " << " ";
}


//****************************************************************************


} // namespace TextUtil

} // namespace Analysis
