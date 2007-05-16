// -*-Mode: C++;-*-
// $Id$

// * BeginRiceCopyright *****************************************************
// 
// Copyright ((c)) 2002, Rice University 
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
//    CSProfile.H
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef CSProfile_hpp 
#define CSProfile_hpp

//************************* System Include Files ****************************

#include <iostream>

//*************************** User Include Files ****************************

#include <include/general.h> 

#include "CSProfTree.hpp"
#include "CSProfMetric.hpp"
#include "CSProfEpoch.hpp"

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
  CSProfile(unsigned int i);
  virtual ~CSProfile();
  
  // Data

  const std::string& GetTarget()             const { return target; }
  unsigned int     GetNumberOfMetrics()      const { return numberofmetrics; }
  CSProfileMetric* GetMetric(unsigned int i) const { return &metrics[i]; }
  CSProfTree*      GetTree()                 const { return tree; }
  void             SetEpoch(CSProfEpoch *ep)  {epoch=ep;}
  CSProfEpoch*     GetEpoch()               const { return epoch; }
  void             ProfileDumpEpoch() {epoch->Dump(); }

  void SetTarget(const char* s) { target = (s) ? s : ""; }

  // Dump contents for inspection
  virtual void Dump(std::ostream& os = std::cerr) const;
  virtual void DDump() const;
 
private:
  std::string target; 
  unsigned int  numberofmetrics;
  CSProfileMetric* metrics;
  CSProfTree* tree;
  CSProfEpoch* epoch;
};

#endif

