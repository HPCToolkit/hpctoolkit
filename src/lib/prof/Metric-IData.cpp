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
// Copyright ((c)) 2002-2019, Rice University
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
//    $HeadURL$
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

//************************* System Include Files ****************************

#include <iostream>

#include <string>
using std::string;

#include <typeinfo>

//*************************** User Include Files ****************************

#include <include/gcc-attr.h>
#include <include/uint.h>

#include "Metric-IData.hpp"

#include <lib/xml/xml.hpp>

#include <lib/support/diagnostics.h>

//*************************** Forward Declarations **************************


//***************************************************************************

namespace Prof {
namespace Metric {


//***************************************************************************
// IData
//***************************************************************************

std::string
IData::toStringMetrics(int oFlags, const char* pfx) const
{
  std::ostringstream os;
  dumpMetrics(os, oFlags, pfx);
  return os.str();
}


std::ostream&
IData::writeMetricsXML(std::ostream& os, uint mBegId, uint mEndId,
		       int GCC_ATTR_UNUSED oFlags, const char* pfx) const
{
  bool wasMetricWritten = false;

  if (mBegId == IData::npos) {
    mBegId = 0;
  }
  mEndId = std::min(numMetrics(), mEndId);

  for (uint i = mBegId; i < mEndId; i++) {
    if (hasMetric(i)) {
      double m = metric(i);
      os << ((!wasMetricWritten) ? pfx : "");
      os << "<M " << "n" << xml::MakeAttrNum(i) 
	 << " v" << xml::MakeAttrNum(m) << "/>";
      wasMetricWritten = true;
    }
  }

  return os;
}


std::ostream&
IData::dumpMetrics(std::ostream& os, int GCC_ATTR_UNUSED oFlags,
		   const char* GCC_ATTR_UNUSED pfx) const
{
  return os;
}


void
IData::ddumpMetrics() const
{
  dumpMetrics();
}


//***************************************************************************

} // namespace Metric
} // namespace Prof

