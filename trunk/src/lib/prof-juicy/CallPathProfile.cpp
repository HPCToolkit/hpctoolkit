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

//*************************** User Include Files ****************************

#include <include/general.h>

#include "CallPathProfile.hpp"

#include <lib/xml/xml.hpp>
using namespace xml;

#include <lib/support/Trace.hpp>

//*************************** Forward Declarations **************************

//***************************************************************************


//***************************************************************************
// CSProfile
//***************************************************************************

CSProfile::CSProfile(uint i)
{
  m_metrics.resize(i);
  for (int i = 0; i < m_metrics.size(); ++i) {
    m_metrics[i] = new CSProfileMetric();
  }
  m_cct  = new CSProfTree;
  m_epoch = NULL;
}


CSProfile::~CSProfile()
{
  for (int i = 0; i < m_metrics.size(); ++i) {
    delete m_metrics[i];
  }
  delete m_cct;
  delete m_epoch;
}


void 
CSProfile::merge(const CSProfile& y)
{
  uint x_numMetrics = numMetrics();

  // merge metrics
  for (int i = 0; i < y.numMetrics(); ++i) {
    const CSProfileMetric* m = y.metric(i);
    m_metrics.push_back(new CSProfileMetric(*m));
  }
  
  // merge epochs... [FIXME]
  
  m_cct->merge(y.cct(), x_numMetrics, y.numMetrics());
}


void 
CSProfile::dump(std::ostream& os) const
{
  // FIXME
}


void 
CSProfile::ddump() const
{
  dump();
}

