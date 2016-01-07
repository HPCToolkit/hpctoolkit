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

#ifndef Analysis_TextUtil_hpp
#define Analysis_TextUtil_hpp

//************************* System Include Files ****************************

#include <iostream>
#include <iomanip>

#include <string>
#include <vector>

//*************************** User Include Files ****************************

#include <include/uint.h>

#include <lib/prof/Metric-Mgr.hpp>

//*************************** Forward Declarations ***************************

//****************************************************************************

namespace Analysis {

namespace TextUtil {


//****************************************************************************
//
//****************************************************************************

class ColumnFormatter {
public:
  // NOTE: Since computed metrics may be of any unit, forcing is only
  // allowed for raw metrics.
  enum Flag {
    Flag_NULL  = 0,
    Flag_ForcePct, // force percent column formatting
    Flag_ForceVal  // force value column formatting
  };

public:
  ColumnFormatter(const Prof::Metric::Mgr& metricMgr,
		  std::ostream& os,
		  int numDecPct, int numDecVal);

  ~ColumnFormatter()
  { }

  // generates a summary of the formatted column for all metrics
  void
  genColHeaderSummary();

  // generates a formatted column for metric id 'mid' (if displayed).
  // The flag can force the column to be displayed as a value
  // (non-percentage) even if it was formatted as a percent and vice versa
  void
  genCol(uint mid, double metricVal, double metricTot, Flag flg = Flag_NULL);

  void
  genCol(uint mid, double metricVal, Flag flg = Flag_ForceVal)
  {
    genCol(mid, metricVal, 0, flg);
  }


  // generate a blank column for metric 'mid'
  void
  genBlankCol(uint mid);

  // generates 'formatted blanks' for all 'displayed' metrics
  void
  genBlankCols()
  {
    m_os << std::setw(m_annotWidthTot) << std::setfill(' ') << " ";
  }

private:
  bool
  isDisplayed(uint mId)
  {
    // m_mMgr.metric(mId)->Display()
    return (m_annotWidth[mId] != 0);
  }


private:
  const Prof::Metric::Mgr& m_mMgr;
  std::ostream& m_os;

  // number of decimals in non-scientific format
  int m_numDecPct; // for percents
  int m_numDecVal; // for values

  std::vector<int>    m_annotWidth;
  int                 m_annotWidthTot;

  std::vector<double> m_sciFmtLoThrsh_pct; // x <  thrsh => scientific format
  std::vector<double> m_sciFmtHiThrsh_pct; // x >= thrsh => scientific format

  std::vector<double> m_sciFmtLoThrsh_val; // x <  thrsh => scientific format
  std::vector<double> m_sciFmtHiThrsh_val; // x >= thrsh => scientific format

  std::vector<bool>   m_dispPercent;
  std::vector<bool>   m_isForceable;
};


} // namespace TextUtil

} // namespace Analysis

//****************************************************************************

#endif // Analysis_TextUtil_hpp
