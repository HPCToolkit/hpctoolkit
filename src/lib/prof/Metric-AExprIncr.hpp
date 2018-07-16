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
// Copyright ((c)) 2002-2018, Rice University
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

#ifndef prof_Prof_Metric_AExprIncr_hpp
#define prof_Prof_Metric_AExprIncr_hpp

//************************ System Include Files ******************************

#include <iostream> 
#include <string>

#include <cfloat>
#include <cmath>

//************************* User Include Files *******************************

#include "Metric-IData.hpp"
#include "Metric-IDBExpr.hpp"

#include <lib/support/diagnostics.h>
#include <lib/support/NaN.h>
#include <lib/support/Unique.hpp>
#include <lib/support/StrUtil.hpp>


//************************ Forward Declarations ******************************

//****************************************************************************

#define epsilon  (0.000001)

namespace Prof {

namespace Metric {

// ----------------------------------------------------------------------
// class AExprIncr
//   The base class for all concrete evaluation classes
// ----------------------------------------------------------------------

class AExprIncr
  : public IDBExpr,
    public Unique // disable copying, for now
{
public:
  // fixme: return this with 'constexpr' after switching to std C++11
  // static const double epsilon = 0.000001;

public:
  AExprIncr(uint accumId, uint srcId)
    : m_numSrcFxd(0), m_numSrcVarId(Metric::IData::npos)
  {
    m_accumId[0] = accumId;
    m_srcId[0] = srcId;
    for (uint i = 1; i < maxAccums; ++i) {
      m_accumId[i] = Metric::IData::npos;
      m_srcId[i] = Metric::IData::npos;
    }
  }

  AExprIncr(uint accumId, uint accum2Id, uint srcId)
    : m_numSrcFxd(0), m_numSrcVarId(Metric::IData::npos)
  {
    m_accumId[0] = accumId;
    m_srcId[0] = srcId;
    m_accumId[1] = accum2Id;
    m_srcId[1] = Metric::IData::npos;
    for (uint i = 2; i < maxAccums; ++i) {
      m_accumId[i] = Metric::IData::npos;
      m_srcId[i] = Metric::IData::npos;
    }
  }

  virtual ~AExprIncr()
  { }


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
  // Metric::IDBExpr: exported formulas for Flat and Callers view
  //   (other functions are distributed below)
  // ------------------------------------------------------------

  virtual std::string
  combineString2() const
  { DIAG_Die(DIAG_Unimplemented); }


  // ------------------------------------------------------------
  // accum:  accumulator
  // accum2: accumulator2 (if needed)
  // ------------------------------------------------------------

  // Metric::IDBExpr
  virtual uint
  accumId(int i) const
  { return m_accumId[i]; }

  void
  accumId(int i, uint x)
  { m_accumId[i] = x; }

  double
  accumVar(int i, const Metric::IData& mdata) const
  { return var(mdata, m_accumId[i]); }

  double&
  accumVar(int i, Metric::IData& mdata) const
  { return var(mdata, m_accumId[i]); }


  // Metric::IDBExpr
  virtual uint
  numAccum() const
  { return 1; }


  // ------------------------------------------------------------
  // srcId: input source for accumulate()
  // ------------------------------------------------------------

  void
  srcId(int i, uint x)
  { m_srcId[i] = x; }

  double
  srcVar(int i, const Metric::IData& mdata) const
  { return var(mdata, m_srcId[i]); }

  double&
  srcVar(int i, Metric::IData& mdata) const
  { return var(mdata, m_srcId[i]); }

  std::string
  srcStr(int i) const
  { return "$"+ StrUtil::toStr(m_srcId[i]); }

  
  bool
  isSetSrc(int i) const
  { return (m_srcId[i] != Metric::IData::npos); }

  // ------------------------------------------------------------
  // numSrcFix: number of inputs for CCT (fixed)
  // numSrcVar: number of inputs, which can be variable
  // ------------------------------------------------------------

  // Metric::IDBExpr
  virtual bool
  hasNumSrcVar() const
  { return false; }

  uint
  numSrc(const Metric::IData& mdata) const
  { return (hasNumSrcVar()) ? numSrcVarVar(mdata) : m_numSrcFxd; }


  // Metric::IDBExpr
  virtual uint
  numSrcFxd() const
  { return m_numSrcFxd; }

  void
  numSrcFxd(uint x)
  { m_numSrcFxd = x; }


  bool
  isSetNumSrcVar() const
  { return (m_numSrcVarId != Metric::IData::npos); }

  // Metric::IDBExpr
  virtual uint
  numSrcVarId() const
  { return m_numSrcVarId; }

  void
  numSrcVarId(uint x)
  { m_numSrcVarId = x; }

  uint
  numSrcVarVar(const Metric::IData& mdata) const
  { return (uint)var(mdata, m_numSrcVarId); }


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


public:
  // ------------------------------------------------------------
  // common functions for standard deviation
  // ------------------------------------------------------------

  double
  initializeStdDev(Metric::IData& mdata) const
  {
    accumVar(0, mdata) = 0.0;
    accumVar(1, mdata) = 0.0;
    return 0.0;
  }


  double
  initializeSrcStdDev(Metric::IData& mdata) const
  {
    srcVar(0, mdata) = 0.0;
    if (isSetSrc(1)) {
      srcVar(1, mdata) = 0.0;
    }
    return 0.0;
  }


  double
  accumulateStdDev(Metric::IData& mdata) const
  {
    double a1 = accumVar(0, mdata), a2 = accumVar(1, mdata), s = srcVar(0, mdata);
    double z1 = a1 + s;       // running sum
    double z2 = a2 + (s * s); // running sum of squares
    accumVar(0, mdata)  = z1;
    accumVar(1, mdata) = z2;
    return z1;
  }


  double
  combineStdDev(Metric::IData& mdata) const
  {
    double a1 = accumVar(0, mdata), a2 = accumVar(1, mdata);
    double s1 = srcVar(0, mdata), s2 = srcVar(1, mdata);
    double z1 = a1 + s1; // running sum
    double z2 = a2 + s2; // running sum of squares
    accumVar(0, mdata)  = z1;
    accumVar(1, mdata) = z2;
    return z1;
  }


  double
  finalizeStdDev(Metric::IData& mdata) const
  {
    double a1 = accumVar(0, mdata);  // running sum
    double a2 = accumVar(1, mdata); // running sum of squares
    double sdev = a1;
    if (numSrc(mdata) > 0) {
      double n = numSrc(mdata);
      double mean = a1 / n;
      double z1 = (mean * mean); // (mean)^2
      double z2 = a2 / n;        // (sum of squares)/n
      sdev = sqrt(z2 - z1);      // stddev

      accumVar(0, mdata) = sdev;
      accumVar(1, mdata) = mean;
    }
    return sdev;
  }


  // ------------------------------------------------------------
  //
  // ------------------------------------------------------------
  
  virtual std::string
  toString() const;


  virtual std::ostream&
  dump(std::ostream& os = std::cout) const
  {
    dumpMe(os);
    return os;
  }

  virtual std::ostream&
  dumpMe(std::ostream& os = std::cout) const = 0;

  // Metric::IDBExpr::ddump()  

protected:
  uint m_accumId[maxAccums];  // accumulators
  uint m_srcId[maxAccums];  // input source for accumulate()
  uint m_numSrcFxd;   // number of inputs (fixed)
  uint m_numSrcVarId; // number of inputs, which can be variable
};


// ----------------------------------------------------------------------
// MinIncr: (observational min instead of absolute min)
// ----------------------------------------------------------------------

// Computes observational min instead of absolute min.  Reports
// DBL_MIN when there have been no obervations.

// In the abstract, we could could use initialize()/initializeSrc() to
// make DBL_MAX the one non-obervational value.  However there are two
// artificial problems with this.  First there are difficulties in
// hpcprof-mpi (see comments associated with 'FnInitSrc' in
// src/tool/hpcprof-mpi/main.cpp).  Second, it requires a finalize
// function string that converts DBL_MAX to DBL_MIN (the preferred
// reported value), which is difficult to generate.
//
// Our solution is to (a) use DBL_MIN and 0.0 as the two
// non-observational values and (b) maintain the accumulator so that
// no conversion at finalization from DBL_MAX to DBL_MIN is required.
// This requires some additional tests within accumulate() and
// combine().


class MinIncr
  : public AExprIncr
{
public:
  MinIncr(uint accumId, uint srcId)
    : AExprIncr(accumId, srcId)
  { }

  virtual ~MinIncr()
  { }


  // ------------------------------------------------------------
  // 
  // ------------------------------------------------------------

  virtual double
  initialize(Metric::IData& mdata) const
  { return (accumVar(0, mdata) = DBL_MIN /* sic; see above */); }

  virtual double
  initializeSrc(Metric::IData& mdata) const
  { return (srcVar(0, mdata) = DBL_MIN /* sic; see above */); }

  virtual double
  accumulate(Metric::IData& mdata) const
  {
    double a = accumVar(0, mdata), s = srcVar(0, mdata);
    double z = a;

    // See comments above
    if (s != DBL_MIN && s != 0.0) {
      z = (a == DBL_MIN) ? s : std::min(a, s);
      accumVar(0, mdata) = z;
    }
    DIAG_MsgIf(0, "MinIncr: min("<< a << ", " << s << ") = " << z);

    return z;
  }

  virtual double
  combine(Metric::IData& mdata) const
  { return MinIncr::accumulate(mdata); }

  virtual double
  finalize(Metric::IData& mdata) const
  { return accumVar(0, mdata); }


  // ------------------------------------------------------------
  // Metric::IDBExpr: exported formulas for Flat and Callers view
  // ------------------------------------------------------------

  virtual std::string
  combineString1() const
  { return combineString1Min(); }

  virtual std::string
  finalizeString() const
  { return finalizeStringMin(); }


  // ------------------------------------------------------------
  // 
  // ------------------------------------------------------------

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


  // ------------------------------------------------------------
  // 
  // ------------------------------------------------------------

  virtual double
  initialize(Metric::IData& mdata) const
  { return (accumVar(0, mdata) = 0.0); }

  virtual double
  initializeSrc(Metric::IData& mdata) const
  { return (srcVar(0, mdata) = 0.0); }

  virtual double
  accumulate(Metric::IData& mdata) const
  {
    double a = accumVar(0, mdata), s = srcVar(0, mdata);
    double z = std::max(a, s);
    DIAG_MsgIf(0, "MaxIncr: max("<< a << ", " << s << ") = " << z);
    accumVar(0, mdata) = z;
    return z;
  }

  virtual double
  combine(Metric::IData& mdata) const
  { return MaxIncr::accumulate(mdata); }

  virtual double
  finalize(Metric::IData& mdata) const
  { return accumVar(0, mdata); }


  // ------------------------------------------------------------
  // Metric::IDBExpr: exported formulas for Flat and Callers view
  // ------------------------------------------------------------

  virtual std::string
  combineString1() const
  { return combineString1Max(); }

  virtual std::string
  finalizeString() const
  { return finalizeStringMax(); }


  // ------------------------------------------------------------
  // 
  // ------------------------------------------------------------

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


  // ------------------------------------------------------------
  //
  // ------------------------------------------------------------

  virtual double
  initialize(Metric::IData& mdata) const
  { return (accumVar(0, mdata) = 0.0); }

  virtual double
  initializeSrc(Metric::IData& mdata) const
  { return (srcVar(0, mdata) = 0.0); }

  virtual double
  accumulate(Metric::IData& mdata) const
  {
    double a = accumVar(0, mdata), s = srcVar(0, mdata);
    double z = a + s;
    DIAG_MsgIf(0, "SumIncr: +("<< a << ", " << s << ") = " << z);
    accumVar(0, mdata) = z;
    return z;
  }

  virtual double
  combine(Metric::IData& mdata) const
  { return SumIncr::accumulate(mdata); }

  virtual double
  finalize(Metric::IData& mdata) const
  { return accumVar(0, mdata); }


  // ------------------------------------------------------------
  // Metric::IDBExpr: exported formulas for Flat and Callers view
  // ------------------------------------------------------------

  virtual std::string
  combineString1() const
  { return combineString1Sum(); }

  virtual std::string
  finalizeString() const
  { return finalizeStringSum(); }


  // ------------------------------------------------------------
  // 
  // ------------------------------------------------------------

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


  // ------------------------------------------------------------
  // 
  // ------------------------------------------------------------

  virtual double
  initialize(Metric::IData& mdata) const
  { return (accumVar(0, mdata) = 0.0); }

  virtual double
  initializeSrc(Metric::IData& mdata) const
  { return (srcVar(0, mdata) = 0.0); }

  virtual double
  accumulate(Metric::IData& mdata) const
  {
    double a = accumVar(0, mdata), s = srcVar(0, mdata);
    double z = a + s;
    DIAG_MsgIf(0, "MeanIncr: +("<< a << ", " << s << ") = " << z);
    accumVar(0, mdata) = z;
    return z;
  }

  virtual double
  combine(Metric::IData& mdata) const
  { return MeanIncr::accumulate(mdata); }

  virtual double
  finalize(Metric::IData& mdata) const
  {
    double a = accumVar(0, mdata);
    double z = a;
    if (numSrc(mdata) > 0) {
      double n = numSrc(mdata);
      z = a / n;
      accumVar(0, mdata) = z;
    }
    return z;
  }


  // ------------------------------------------------------------
  // Metric::IDBExpr: exported formulas for Flat and Callers view
  // ------------------------------------------------------------

  virtual bool
  hasNumSrcVar() const
  { return true; }

  virtual std::string
  combineString1() const
  { return combineString1Mean(); }

  virtual std::string
  finalizeString() const
  { return finalizeStringMean(); }


  // ------------------------------------------------------------
  // 
  // ------------------------------------------------------------

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


  // ------------------------------------------------------------
  // 
  // ------------------------------------------------------------

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


  // ------------------------------------------------------------
  // Metric::IDBExpr: exported formulas for Flat and Callers view
  // ------------------------------------------------------------

  virtual uint
  numAccum() const
  { return 2; }

  virtual bool
  hasNumSrcVar() const
  { return true; }

  virtual std::string
  combineString1() const
  { return combineString1StdDev(); }

  virtual std::string
  combineString2() const
  { return combineString2StdDev(); }

  virtual std::string
  finalizeString() const
  { return finalizeStringStdDev(); }


  // ------------------------------------------------------------
  // 
  // ------------------------------------------------------------

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


  // ------------------------------------------------------------
  // 
  // ------------------------------------------------------------

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
    double mean = accumVar(1, mdata);
    double z = 0.0;
    if (mean > epsilon) {
      z = sdev / mean;
    }
    accumVar(0, mdata) = z;
    return z;
  }


  // ------------------------------------------------------------
  // Metric::IDBExpr: exported formulas for Flat and Callers view
  // ------------------------------------------------------------

  virtual int
  hasAccum() const
  { return 2; }

  virtual bool
  hasNumSrcVar() const
  { return true; }

  virtual std::string
  combineString1() const
  { return combineString1StdDev(); }

  virtual std::string
  combineString2() const
  { return combineString2StdDev(); }

  virtual std::string
  finalizeString() const
  { return finalizeStringCoefVar(); }


  // ------------------------------------------------------------
  // 
  // ------------------------------------------------------------

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


  // ------------------------------------------------------------
  // 
  // ------------------------------------------------------------

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
    double mean = accumVar(1, mdata);
    double z = 0.0;
    if (mean > epsilon) {
      z = (sdev / mean) * 100;
    }
    accumVar(0, mdata) = z;
    return z;
  }


  // ------------------------------------------------------------
  // Metric::IDBExpr: exported formulas for Flat and Callers view
  // ------------------------------------------------------------

  virtual uint
  numAccum() const
  { return 2; }

  virtual bool
  hasNumSrcVar() const
  { return true; }

  virtual std::string
  combineString1() const
  { return combineString1StdDev(); }

  virtual std::string
  combineString2() const
  { return combineString2StdDev(); }

  virtual std::string
  finalizeString() const
  { return finalizeStringRStdDev(); }


  // ------------------------------------------------------------
  // 
  // ------------------------------------------------------------

  virtual std::ostream&
  dumpMe(std::ostream& os = std::cout) const;

private:
};


// ----------------------------------------------------------------------
// NumSourceIncr: A special metric for representing the number of
// inputs for an AExprIncr.
// ----------------------------------------------------------------------

class NumSourceIncr
  : public AExprIncr
{
public:
  NumSourceIncr(uint accumId, uint srcId)
    : AExprIncr(accumId, srcId)
  { }

  virtual ~NumSourceIncr()
  { }


  // ------------------------------------------------------------
  //
  // ------------------------------------------------------------

  virtual double
  initialize(Metric::IData& mdata) const
  {
    double z = accumVar(0, mdata) = numSrcFxd();
    return z;
  }

  virtual double
  initializeSrc(Metric::IData& GCC_ATTR_UNUSED mdata) const
  { return 0.0; }

  virtual double
  accumulate(Metric::IData& mdata) const
  { return accumVar(0, mdata); }

  virtual double
  combine(Metric::IData& mdata) const
  { return accumVar(0, mdata); }

  virtual double
  finalize(Metric::IData& mdata) const
  { return accumVar(0, mdata); }


  // ------------------------------------------------------------
  // Metric::IDBExpr: exported formulas for Flat and Callers view
  // ------------------------------------------------------------

  virtual std::string
  combineString1() const
  { return combineString1NumSource(); }

  virtual std::string
  finalizeString() const
  { return finalizeStringNumSource(); }


  // ------------------------------------------------------------
  // 
  // ------------------------------------------------------------

  virtual std::ostream&
  dumpMe(std::ostream& os = std::cout) const;

private:
};

//****************************************************************************

} // namespace Metric

} // namespace Prof

//****************************************************************************

#endif /* prof_Prof_Metric_AExprIncr_hpp */
