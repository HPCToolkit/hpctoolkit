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
// Copyright ((c)) 2002-2009, Rice University 
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
// class Prof::Metric::AExpr and derived classes
//
// An abstract expression that represents derived expressions that are
// incrementally computed because all inputs are not available at one
// time.
// 
// Since all sources are *not* known in advance, it is necessary to
// use an 'accumulator' that is incrementally updated and serves as both
// an input and output on each update.  This implies that is is
// necessary, in general, to have an initialization routine so the
// accumulator is correctly initialized before the first update.
// Additionally, since some metrics rely on the total number of
// inputs, a finalization routine is also necessary.
//
// Currently supported expressions are
//   MinIncr    : min expression
//   MaxIncr    : max expression
//   SumIncr    : sum expression
//   MeanIncr   : mean (arithmetic) expression
//   StdDevIncr : standard deviation expression
//   CoefVarIncr: coefficient of variance
//   RStdDevIncr: relative standard deviation
//
//***************************************************************************

#ifndef prof_juicy_Prof_Metric_AExprIncr_hpp
#define prof_juicy_Prof_Metric_AExprIncr_hpp

//************************ System Include Files ******************************

#include <iostream> 
#include <string>

#include <cfloat>
#include <cmath>

//************************* User Include Files *******************************

#include "Metric-IData.hpp"

#include <lib/support/diagnostics.h>
#include <lib/support/NaN.h>
#include <lib/support/Unique.hpp>
#include <lib/support/StrUtil.hpp>


//************************ Forward Declarations ******************************

//****************************************************************************

namespace Prof {

namespace Metric {

// ----------------------------------------------------------------------
// class AExprIncr
//   The base class for all concrete evaluation classes
// ----------------------------------------------------------------------

class AExprIncr
  : public Unique // disable copying, for now
{
public:
  static const double epsilon = 0.000001;

public:
  AExprIncr(uint accumId, uint srcId)
    : m_accumId(accumId), m_accum2Id(Metric::IData::npos),
      m_srcId(srcId), m_src2Id(Metric::IData::npos),
      m_numSrcFxd(0), m_numSrcVarId(Metric::IData::npos)
  { }

  AExprIncr(uint accumId, uint accum2Id, uint srcId)
    : m_accumId(accumId), m_accum2Id(accum2Id),
      m_srcId(srcId), m_src2Id(Metric::IData::npos),
      m_numSrcFxd(0), m_numSrcVarId(Metric::IData::npos)
  { }

  virtual ~AExprIncr()
  { }


  // ------------------------------------------------------------
  // accum:  accumulator
  // accum2: accumulator2 (if needed)
  // ------------------------------------------------------------

  void
  accumId(uint x)
  { m_accumId = x; }

  double
  accumVar(const Metric::IData& mdata) const
  { return var(mdata, m_accumId); }

  double&
  accumVar(Metric::IData& mdata) const
  { return var(mdata, m_accumId); }

  std::string
  accumStr() const
  { return "$"+ StrUtil::toStr(m_accumId); }


  bool
  hasAccum2() const
  { return (m_accum2Id != Metric::IData::npos); }

  uint
  accum2Id()
  { return m_accum2Id; }

  void
  accum2Id(uint x)
  { m_accum2Id = x; }

  double
  accum2Var(const Metric::IData& mdata) const
  { return var(mdata, m_accum2Id); }

  double&
  accum2Var(Metric::IData& mdata) const
  { return var(mdata, m_accum2Id); }

  std::string
  accum2Str() const
  { return "$"+ StrUtil::toStr(m_accum2Id); }


  // ------------------------------------------------------------
  // srcId: input source for accumulate()
  // src2Id: input source for accumulator 2's combine()
  // ------------------------------------------------------------

  void
  srcId(uint x)
  { m_srcId = x; }

  double
  srcVar(const Metric::IData& mdata) const
  { return var(mdata, m_srcId); }

  double&
  srcVar(Metric::IData& mdata) const
  { return var(mdata, m_srcId); }

  std::string
  srcStr() const
  { return "$"+ StrUtil::toStr(m_srcId); }

  
  bool
  hasSrc2() const
  { return (m_src2Id != Metric::IData::npos); }

  void
  src2Id(uint x)
  { m_src2Id = x; }

  double
  src2Var(const Metric::IData& mdata) const
  { return var(mdata, m_src2Id); }

  double&
  src2Var(Metric::IData& mdata) const
  { return var(mdata, m_src2Id); }

  std::string
  src2Str() const
  { return "$" + StrUtil::toStr(m_src2Id); }


  // ------------------------------------------------------------
  // numSrcFix: number of inputs for CCT (fixed)
  // numSrcVar: number of inputs, which can be variable
  // ------------------------------------------------------------

  uint
  numSrc(const Metric::IData& mdata) const
  { return (hasNumSrcVar()) ? numSrcVarVar(mdata) : m_numSrcFxd; }

  std::string
  numSrcStr() const
  { return (hasNumSrcVar()) ? numSrcVarStr() : numSrcFxdStr(); }


  uint
  numSrcFxd() const
  { return m_numSrcFxd; }

  void
  numSrcFxd(uint x)
  { m_numSrcFxd = x; }

  std::string
  numSrcFxdStr() const
  { return StrUtil::toStr(m_numSrcFxd); }



  bool
  hasNumSrcVar() const
  { return (m_numSrcVarId != Metric::IData::npos); }

  void
  numSrcVarId(uint x)
  { m_numSrcVarId = x; }

  uint
  numSrcVarVar(const Metric::IData& mdata) const
  { return (uint)var(mdata, m_numSrcVarId); }

  std::string
  numSrcVarStr() const
  { return "$" + StrUtil::toStr(m_numSrcVarId); }


  // ------------------------------------------------------------
  // functions for computing a metric incrementally
  // ------------------------------------------------------------

  enum FnTy { FnInit, FnInitSrc, FnAccum, FnCombine, FnFini };

  // initialize: initializes destination metrics (accumVar() & accum2Var())
  virtual double
  initialize(Metric::IData& mdata) const = 0;

  // initializeSrc: initializes source metrics (srcVar() & srcVar2())
  virtual double
  initializeSrc(Metric::IData& mdata) const = 0;

  // accumulate: updates accumulators using an individual source, srcVar().
  virtual double
  accumulate(Metric::IData& mdata) const = 0;

  // combine: updates accumulators with sources that themselves
  // represent accumulators.  There is one source for each accumulator
  // (srcVar() & srcVar2()).
  virtual double
  combine(Metric::IData& mdata) const = 0;

  // finalize: finalizes destination metrics using numSrc()
  virtual double
  finalize(Metric::IData& mdata) const = 0;


  // ------------------------------------------------------------
  // R- or L-Value of variable reference (cf. AExpr::Var)
  // ------------------------------------------------------------

  static double&
  var(Metric::IData& mdata, uint mId)
  { return mdata.demandMetric(mId); }

  static double
  var(const Metric::IData& mdata, uint mId)
  { return mdata.demandMetric(mId); }


  // ------------------------------------------------------------
  //
  // ------------------------------------------------------------

  static bool
  isok(double x)
  { return !(c_isnan_d(x) || c_isinf_d(x)); }


  // ------------------------------------------------------------
  // 
  // ------------------------------------------------------------

  double
  initializeStdDev(Metric::IData& mdata) const
  {
    accumVar(mdata) = 0.0;
    accum2Var(mdata) = 0.0;
    return 0.0;
  }


  double
  initializeSrcStdDev(Metric::IData& mdata) const
  {
    srcVar(mdata) = 0.0;
    if (hasSrc2()) {
      src2Var(mdata) = 0.0;
    }
    return 0.0;
  }


  double
  accumulateStdDev(Metric::IData& mdata) const
  {
    double a1 = accumVar(mdata), a2 = accum2Var(mdata), s = srcVar(mdata);
    double z1 = a1 + s;       // running sum
    double z2 = a2 + (s * s); // running sum of squares
    accumVar(mdata)  = z1;
    accum2Var(mdata) = z2;
    return z1;
  }


  double
  combineStdDev(Metric::IData& mdata) const
  {
    double a1 = accumVar(mdata), a2 = accum2Var(mdata);
    double s1 = srcVar(mdata), s2 = src2Var(mdata);
    double z1 = a1 + s1; // running sum
    double z2 = a2 + s2; // running sum of squares
    accumVar(mdata)  = z1;
    accum2Var(mdata) = z2;
    return z1;
  }


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


  double
  finalizeStdDev(Metric::IData& mdata) const
  {
    double a1 = accumVar(mdata);  // running sum
    double a2 = accum2Var(mdata); // running sum of squares
    double sdev = a1;
    if (numSrc(mdata) > 0) {
      double n = numSrc(mdata);
      double mean = a1 / n;
      double z1 = (mean * mean); // (mean)^2
      double z2 = a2 / n;        // (sum of squares)/n
      sdev = sqrt(z2 - z1);      // stddev

      accumVar(mdata) = sdev;
      accum2Var(mdata) = mean;
    }
    return sdev;
  }


  std::string
  finalizeStringStdDev(std::string* meanRet = NULL) const
  {
    std::string n = numSrcStr();
    std::string a1 = accumStr();  // running sum
    std::string a2 = accum2Str(); // running sum of squares

    std::string mean = "(" + a1 + " / " + n + ")";
    std::string z1 = "(pow(" + mean + ", 2)"; // (mean)^2
    std::string z2 = "(" + a2 + " / " + n  + ")";      // (sum of squares)/n
    std::string sdev = "sqrt(" + z2 + " - " + z1 + ")";

    if (meanRet) {
      *meanRet = mean;
    }
    return sdev;
  }


  // ------------------------------------------------------------
  //
  // ------------------------------------------------------------
  
  virtual std::string
  toString() const;

  virtual std::string
  combineString1() const
  { DIAG_Die(DIAG_Unimplemented); }

  virtual std::string
  combineString2() const
  { DIAG_Die(DIAG_Unimplemented); }

  virtual std::string
  finalizeString() const
  { DIAG_Die(DIAG_Unimplemented); }


  std::ostream&
  dump(std::ostream& os = std::cerr) const
  {
    dumpMe(os);
    return os;
  }

  virtual std::ostream&
  dumpMe(std::ostream& os = std::cout) const = 0;
  
  void
  ddump() const;

protected:
  uint m_accumId;  // accumulator 1
  uint m_accum2Id; // accumulator 2 (if needed)

  uint m_srcId;  // input source for accumulate()
  uint m_src2Id; // input source for accumulator 2's combine()

  uint m_numSrcFxd;   // number of inputs (fixed)
  uint m_numSrcVarId; // number of inputs, which can be variable
};


// ----------------------------------------------------------------------
// MinIncr
// ----------------------------------------------------------------------

class MinIncr
  : public AExprIncr
{
public:
  MinIncr(uint accumId, uint srcId)
    : AExprIncr(accumId, srcId)
  { }

  virtual ~MinIncr()
  { }


  virtual double
  initialize(Metric::IData& mdata) const
  { return (accumVar(mdata) = DBL_MAX); }

  virtual double
  initializeSrc(Metric::IData& mdata) const
  { return (srcVar(mdata) = DBL_MAX); }

  virtual double
  accumulate(Metric::IData& mdata) const
  {
    double a = accumVar(mdata), s = srcVar(mdata);
    double z = std::min(a, s);
    DIAG_MsgIf(0, "MinIncr: min("<< a << ", " << s << ") = " << z);
    accumVar(mdata) = z;
    return z;
  }

  virtual double
  combine(Metric::IData& mdata) const
  { return MinIncr::accumulate(mdata); }

  virtual double
  finalize(Metric::IData& mdata) const
  { return accumVar(mdata); }


  virtual std::string
  combineString1() const
  {
    std::string a = accumStr();
    std::string z = "min(" + a + ", " + a + ")";
    return z;
  }

  virtual std::string
  finalizeString() const
  { return accumStr(); }

  virtual std::ostream&
  dumpMe(std::ostream& os = std::cout) const;

private:
};


// ----------------------------------------------------------------------
// MaxIncr
// ----------------------------------------------------------------------

class MaxIncr
  : public AExprIncr
{
public:
  MaxIncr(uint accumId, uint srcId)
    : AExprIncr(accumId, srcId)
  { }

  virtual ~MaxIncr()
  { }


  virtual double
  initialize(Metric::IData& mdata) const
  { return (accumVar(mdata) = 0.0); }

  virtual double
  initializeSrc(Metric::IData& mdata) const
  { return (srcVar(mdata) = 0.0); }

  virtual double
  accumulate(Metric::IData& mdata) const
  {
    double a = accumVar(mdata), s = srcVar(mdata);
    double z = std::max(a, s);
    DIAG_MsgIf(0, "MaxIncr: max("<< a << ", " << s << ") = " << z);
    accumVar(mdata) = z;
    return z;
  }

  virtual double
  combine(Metric::IData& mdata) const
  { return MaxIncr::accumulate(mdata); }

  virtual double
  finalize(Metric::IData& mdata) const
  { return accumVar(mdata); }


  virtual std::string
  combineString1() const
  {
    std::string a = accumStr();
    std::string z = "max(" + a + ", " + a + ")";
    return z;
  }

  virtual std::string
  finalizeString() const
  { return accumStr(); }

  virtual std::ostream&
  dumpMe(std::ostream& os = std::cout) const;

private:
};


// ----------------------------------------------------------------------
// SumIncr
// ----------------------------------------------------------------------

class SumIncr
  : public AExprIncr
{
public:
  SumIncr(uint accumId, uint srcId)
    : AExprIncr(accumId, srcId)
  { }

  virtual ~SumIncr()
  { }


  virtual double
  initialize(Metric::IData& mdata) const
  { return (accumVar(mdata) = 0.0); }

  virtual double
  initializeSrc(Metric::IData& mdata) const
  { return (srcVar(mdata) = 0.0); }

  virtual double
  accumulate(Metric::IData& mdata) const
  {
    double a = accumVar(mdata), s = srcVar(mdata);
    double z = a + s;
    DIAG_MsgIf(0, "SumIncr: +("<< a << ", " << s << ") = " << z);
    accumVar(mdata) = z;
    return z;
  }

  virtual double
  combine(Metric::IData& mdata) const
  { return SumIncr::accumulate(mdata); }

  virtual double
  finalize(Metric::IData& mdata) const
  { return accumVar(mdata); }


  virtual std::string
  combineString1() const
  {
    std::string a = accumStr();
    std::string z = "sum(" + a + ", " + a + ")";
    return z;
  }

  virtual std::string
  finalizeString() const
  { return accumStr(); }

  virtual std::ostream&
  dumpMe(std::ostream& os = std::cout) const;

private:
};


// ----------------------------------------------------------------------
// MeanIncr
// ----------------------------------------------------------------------

class MeanIncr
  : public AExprIncr
{
public:
  MeanIncr(uint accumId, uint srcId)
    : AExprIncr(accumId, srcId)
  { }

  virtual ~MeanIncr()
  { }


  virtual double
  initialize(Metric::IData& mdata) const
  { return (accumVar(mdata) = 0.0); }

  virtual double
  initializeSrc(Metric::IData& mdata) const
  { return (srcVar(mdata) = 0.0); }

  virtual double
  accumulate(Metric::IData& mdata) const
  {
    double a = accumVar(mdata), s = srcVar(mdata);
    double z = a + s;
    DIAG_MsgIf(0, "MeanIncr: +("<< a << ", " << s << ") = " << z);
    accumVar(mdata) = z;
    return z;
  }

  virtual double
  combine(Metric::IData& mdata) const
  { return MeanIncr::accumulate(mdata); }

  virtual double
  finalize(Metric::IData& mdata) const
  {
    double a = accumVar(mdata);
    double z = a;
    if (numSrc(mdata) > 0) {
      double n = numSrc(mdata);
      z = a / n;
      accumVar(mdata) = z;
    }
    return z;
  }


  virtual std::string
  combineString1() const
  {
    std::string a = accumStr();
    std::string z = "sum(" + a + ", " + a + ")";
    return z;
  }

  virtual std::string
  finalizeString() const
  {
    std::string n = numSrcStr();
    std::string a = accumStr();
    std::string z = a + " / " + n;
    return z;
  }

  virtual std::ostream&
  dumpMe(std::ostream& os = std::cout) const;

private:
};


// ----------------------------------------------------------------------
// StdDevIncr: standard deviation
// ----------------------------------------------------------------------

class StdDevIncr 
  : public AExprIncr
{
public:
  StdDevIncr(uint accumId, uint accum2Id, uint srcId)
    : AExprIncr(accumId, accum2Id, srcId)
  { }

  virtual ~StdDevIncr()
  { }


  virtual double
  initialize(Metric::IData& mdata) const
  { return initializeStdDev(mdata); }

  virtual double
  initializeSrc(Metric::IData& mdata) const
  { return initializeSrcStdDev(mdata); }

  virtual double
  accumulate(Metric::IData& mdata) const
  { return accumulateStdDev(mdata); }

  virtual double
  combine(Metric::IData& mdata) const
  { return combineStdDev(mdata); }

  virtual double
  finalize(Metric::IData& mdata) const
  { return finalizeStdDev(mdata); }


  virtual std::string
  combineString1() const
  { return combineString1StdDev(); }

  virtual std::string
  combineString2() const
  { return combineString2StdDev(); }

  virtual std::string
  finalizeString() const
  { return finalizeStringStdDev(); }

  virtual std::ostream&
  dumpMe(std::ostream& os = std::cout) const;

private:
};


// ----------------------------------------------------------------------
// CoefVarIncr: relative standard deviation
// ----------------------------------------------------------------------

class CoefVarIncr 
  : public AExprIncr
{
public:
  CoefVarIncr(uint accumId, uint accum2Id, uint srcId)
    : AExprIncr(accumId, accum2Id, srcId)
  { }

  virtual ~CoefVarIncr()
  { }


  virtual double
  initialize(Metric::IData& mdata) const
  { return initializeStdDev(mdata); }

  virtual double
  initializeSrc(Metric::IData& mdata) const
  { return initializeSrcStdDev(mdata); }

  virtual double
  accumulate(Metric::IData& mdata) const
  { return accumulateStdDev(mdata); }

  virtual double
  combine(Metric::IData& mdata) const
  { return combineStdDev(mdata); }

  virtual double
  finalize(Metric::IData& mdata) const
  {
    double sdev = finalizeStdDev(mdata);
    double mean = accum2Var(mdata);
    double z = 0.0;
    if (mean > epsilon) {
      z = sdev / mean;
    }
    accumVar(mdata) = z;
    return z;
  }


  virtual std::string
  combineString1() const
  { return combineString1StdDev(); }

  virtual std::string
  combineString2() const
  { return combineString2StdDev(); }

  virtual std::string
  finalizeString() const
  {
    std::string mean;
    std::string sdev = finalizeStringStdDev(&mean);
    std::string z = "(" + sdev + " / " + mean + ")";
    return z;
  }


  virtual std::ostream&
  dumpMe(std::ostream& os = std::cout) const;

private:
};


// ----------------------------------------------------------------------
// RStdDevIncr: relative standard deviation
// ----------------------------------------------------------------------

class RStdDevIncr 
  : public AExprIncr
{
public:
  RStdDevIncr(uint accumId, uint accum2Id, uint srcId)
    : AExprIncr(accumId, accum2Id, srcId)
  { }

  virtual ~RStdDevIncr()
  { }


  virtual double
  initialize(Metric::IData& mdata) const
  { return initializeStdDev(mdata); }

  virtual double
  initializeSrc(Metric::IData& mdata) const
  { return initializeSrcStdDev(mdata); }

  virtual double
  accumulate(Metric::IData& mdata) const
  { return accumulateStdDev(mdata); }

  virtual double
  combine(Metric::IData& mdata) const
  { return combineStdDev(mdata); }

  virtual double
  finalize(Metric::IData& mdata) const
  {
    double sdev = finalizeStdDev(mdata);
    double mean = accum2Var(mdata);
    double z = 0.0;
    if (mean > epsilon) {
      z = (sdev / mean) * 100;
    }
    accumVar(mdata) = z;
    return z;
  }


  virtual std::string
  combineString1() const
  { return combineString1StdDev(); }

  virtual std::string
  combineString2() const
  { return combineString2StdDev(); }

  virtual std::string
  finalizeString() const
  {
    std::string mean;
    std::string sdev = finalizeStringStdDev(&mean);
    std::string z = "(" + sdev + " / " + mean + ") * 100";
    return z;
  }

  virtual std::ostream&
  dumpMe(std::ostream& os = std::cout) const;

private:
};

//****************************************************************************

} // namespace Metric

} // namespace Prof

//****************************************************************************

#endif /* prof_juicy_Prof_Metric_AExprIncr_hpp */
