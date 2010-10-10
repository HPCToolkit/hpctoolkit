// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2010, Rice University 
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

#ifndef prof_Prof_Metric_IDBExpr_hpp 
#define prof_Prof_Metric_IDBExpr_hpp

//************************* System Include Files ****************************

#include <iostream>

#include <string>
 
//*************************** User Include Files ****************************

#include <include/uint.h>

#include <lib/support/diagnostics.h>


//*************************** Forward Declarations **************************


//***************************************************************************

namespace Prof {

namespace Metric {

//***************************************************************************
//
// IDBExpr
//
// Interface/Mixin to represent the Experiment database's formulas for
// computing the Flat and Callers view given derived CCT metrics.
//
//***************************************************************************

class IDBExpr {
public:
  // --------------------------------------------------------
  // Create/Destroy
  // --------------------------------------------------------
#if 0
  IDBExpr()
  { }

  virtual ~IDBExpr()
  { }
  
  IDBExpr&
  operator=(const IDBExpr& x)
  { }
#endif

  // --------------------------------------------------------
  // 
  // --------------------------------------------------------

  virtual bool
  hasAccum2() const = 0;

  // --------------------------------------------------------
  // Formulas to compute Flat and Callers view
  // --------------------------------------------------------

  // initialize: [Flat|Callers]-accum is initialized from CCT-accum

  // combineString1: [Flat|Callers]-accum x cct-accum -> [Flat|Callers]-accum
  virtual std::string
  combineString1() const = 0;

  // combineString2: [Flat|Callers]-accum x cct-accum -> [Flat|Callers]-accum
  virtual std::string
  combineString2() const = 0;

  // finalizeString: accumulator-list -> output
  virtual std::string
  finalizeString() const = 0;
  
private:
};

//***************************************************************************

} // namespace Metric

} // namespace Prof


#endif /* prof_Prof_Metric_IDBExpr_hpp */
