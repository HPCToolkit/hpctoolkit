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

#ifndef prof_juicy_CallPathProfile 
#define prof_juicy_CallPathProfile

//************************* System Include Files ****************************

#include <iostream>
#include <vector>

//*************************** User Include Files ****************************

#include <include/general.h>

#include "CallingContextTree.hpp"
#include "CallPathMetric.hpp"
#include "CallPathEpoch.hpp"

//*************************** Forward Declarations ***************************

//***************************************************************************
// CSProfile
//***************************************************************************

// 'CSProfile' represents data resulting from a call stack profile.
// It assumes that only one (possibly complex) metric was used in
// collecting call stack samples and that consequently there is only
// one set of sample data.  The call stack data is stored as a tree
// with the root representing the bottom of the all the call stack
// samples. ----OLD

// call stack profile could have multiple metrics such as "retired
// instruction" "byte allocated" "byte deallocated" "sigmentation
// fault" "cycle" etc.
// We supposed to get the number of metrics from the data file and
// based on the number to have a vector of metrics
// The call stack data is stored as a tree with the root representing
// the bottom of the all the call stack smples
// muliple metrics => multiple sample data associated with each tree leaf

class CSProfile: public Unique {
public:
  // Constructor/Destructor
  CSProfile(uint i);
  virtual ~CSProfile();
  
  // -------------------------------------------------------
  // Data
  // -------------------------------------------------------
  const std::string& name() const { return m_name; }
  void               name(const char* s) { m_name = (s) ? s : ""; }

  uint             numMetrics() const   { return m_metrics.size(); }
  CSProfileMetric* metric(uint i) const { return m_metrics[i]; }

  CSProfTree*  cct() const { return m_cct; }

  CSProfEpoch* epoch() const         { return m_epoch; }
  void         epoch(CSProfEpoch* x) { m_epoch = x; }


  // -------------------------------------------------------
  // Given a CSProfile, merge into 'this'
  // -------------------------------------------------------
  void merge(const CSProfile& x);

  // -------------------------------------------------------
  // Dump contents for inspection
  // -------------------------------------------------------
  virtual void dump(std::ostream& os = std::cerr) const;
  virtual void ddump() const;
 
private:
  std::string m_name;

  CSProfTree* m_cct;
  std::vector<CSProfileMetric*> m_metrics;
  CSProfEpoch* m_epoch;
};

//***************************************************************************

#endif /* prof_juicy_CallPathProfile */

