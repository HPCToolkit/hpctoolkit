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

#ifndef Analysis_Util_hpp 
#define Analysis_Util_hpp

//************************* System Include Files ****************************

#include <string>
#include <vector>

//*************************** User Include Files ****************************

#include <include/general.h>

#include <lib/prof-juicy/MetricDescMgr.hpp>

//*************************** Forward Declarations ***************************

//****************************************************************************

namespace Analysis {

namespace TextUtil {


//****************************************************************************
//
//****************************************************************************

class ColumnFormatter {
public:
  ColumnFormatter(const Prof::MetricDescMgr& metricMgr, 
		  ostream& os, int num_digits);
  ~ColumnFormatter() { }

  // generates a formatted column for metric id 'mid' (if displayed).
  void 
  genCol(uint mid, uint64_t metricVal, uint64_t metricTot);

  // generates 'formatted blanks' for all 'displayed' metrics
  void
  genBlankCols() {
    m_os << std::setw(m_metricAnnotationWidthTot) << std::setfill(' ') << " ";
  }

  bool
  isDisplayed(uint mId) 
  {
    // m_mMgr.metric(mId)->Display()
    return (m_metricAnnotationWidth[mId] != 0);
  }
  

private:
  const Prof::MetricDescMgr& m_mMgr;
  ostream& m_os;
  int      m_num_digits;

  std::vector<int>    m_metricAnnotationWidth;
  int                 m_metricAnnotationWidthTot;
  std::vector<double> m_scientificFormatThreshold;
};


} // namespace TextUtil

} // namespace Analysis

//****************************************************************************

#endif // Analysis_TextUtil_hpp
