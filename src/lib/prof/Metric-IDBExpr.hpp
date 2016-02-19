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

#ifndef prof_Prof_Metric_IDBExpr_hpp 
#define prof_Prof_Metric_IDBExpr_hpp

//************************* System Include Files ****************************

#include <iostream>

#include <string>
 
//*************************** User Include Files ****************************

#include <include/gcc-attr.h>
#include <include/uint.h>

#include <lib/support/diagnostics.h>
#include <lib/support/StrUtil.hpp>


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
  IDBExpr()
  { }

  virtual ~IDBExpr()
  { }

  IDBExpr&
  operator=(const IDBExpr& GCC_ATTR_UNUSED x)
  { return *this; }


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


  // --------------------------------------------------------
  // Commonly used standard deviation formulas
  // --------------------------------------------------------

  std::string
  combineString1Min() const
  {
    std::string a = accumStr();
    std::string z = "min(" + a + ", " + a + ")";
    return z;
  }

  std::string
  finalizeStringMin() const
  { return accumStr(); }


  std::string
  combineString1Max() const
  {
    std::string a = accumStr();
    std::string z = "max(" + a + ", " + a + ")";
    return z;
  }

  std::string
  finalizeStringMax() const
  { return accumStr(); }


  std::string
  combineString1StdDev() const
  {
    std::string a1 = accumStr();
    std::string z1 = "sum(" + a1 + ", " + a1 + ")"; // running sum
    return z1;
  }

  std::string
  combineString2StdDev() const
  {
    std::string a2 = accum2Str();
    std::string z2 = "sum(" + a2 + ", " + a2 + ")"; // running sum of squares
    return z2;
  }


  std::string
  combineString1Sum() const
  {
    std::string a = accumStr();
    std::string z = "sum(" + a + ", " + a + ")";
    return z;
  }

  std::string
  finalizeStringSum() const
  { return accumStr(); }


  std::string
  combineString1Mean() const
  {
    std::string a = accumStr();
    std::string z = "sum(" + a + ", " + a + ")";
    return z;
  }

  std::string
  finalizeStringMean() const
  {
    // Laks hack: for callers view and flat view, it would be
    // more accurate if we divide the sum with the aggregate
    // since the num of callers view and flat view will be 
    // accumulated
    std::string n = numSrcStr();
    std::string a = accumStr();
    std::string z = a + " / " + n;
    return z;
  }


  std::string
  finalizeStringStdDev(std::string* meanRet = NULL) const
  {
    std::string n = numSrcStr();
    std::string a1 = accumStr();  // running sum
    std::string a2 = accum2Str(); // running sum of squares

    std::string mean = a1 + " / " + n;
    std::string z1 = "pow(" + mean + ", 2)"; // (mean)^2
    std::string z2 = "(" + a2 + " / " + n  + ")";      // (sum of squares)/n
    std::string sdev = "sqrt(" + z2 + " - " + z1 + ")";

    if (meanRet) {
      *meanRet = mean;
    }
    return sdev;
  }


  std::string
  finalizeStringCoefVar() const
  {
    std::string mean;
    std::string sdev = finalizeStringStdDev(&mean);
    std::string z = sdev + " / (" + mean + ")";
    return z;
  }


  std::string
  finalizeStringRStdDev() const
  {
    std::string mean;
    std::string sdev = finalizeStringStdDev(&mean);
    std::string z = sdev + "* 100 / (" + mean + ")";
    return z;
  }


  std::string
  combineString1NumSource() const
  {
    std::string a = accumStr();
    // laks: avoid accumulation of NumSrc for callers view and flat view
    // std::string z = "sum(" + a + ", " + a + ")"; // a + numSrcFix()
    return a; // originally: z;
  }

  std::string
  finalizeStringNumSource() const
  { return accumStr(); }


  // --------------------------------------------------------
  // Primitives for building formulas
  // --------------------------------------------------------

  virtual uint
  accumId() const = 0;

  std::string
  accumStr() const
  { return "$"+ StrUtil::toStr(accumId()); }


  // --------------------------------------------------------
  // Primitives for building formulas
  // --------------------------------------------------------

  virtual bool
  hasAccum2() const = 0;

  virtual uint
  accum2Id() const = 0;

  std::string
  accum2Str() const
  { return "$"+ StrUtil::toStr(accum2Id()); }


  // --------------------------------------------------------
  // Primitives for building formulas
  // --------------------------------------------------------

  virtual bool
  hasNumSrcVar() const = 0;

  std::string
  numSrcStr() const
  { return (hasNumSrcVar()) ? numSrcVarStr() : numSrcFxdStr(); }


  virtual uint
  numSrcFxd() const = 0;

  std::string
  numSrcFxdStr() const
  { return StrUtil::toStr(numSrcFxd()); }


  virtual uint
  numSrcVarId() const = 0;

  std::string
  numSrcVarStr() const
  { return "$" + StrUtil::toStr(numSrcVarId()); }


  // --------------------------------------------------------
  // 
  // --------------------------------------------------------

  virtual std::string
  toString() const;


  virtual std::ostream&
  dump(std::ostream& os = std::cout) const
  { return os; }

  void
  ddump() const;
  
private:
};

//***************************************************************************

} // namespace Metric

} // namespace Prof


#endif /* prof_Prof_Metric_IDBExpr_hpp */
