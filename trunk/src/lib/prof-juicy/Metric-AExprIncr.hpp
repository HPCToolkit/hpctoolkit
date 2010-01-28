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
  AExprIncr(uint dstId, uint srcId)
    : m_dstId(dstId), m_dst2Id(Metric::IData::npos),
      m_srcId(srcId), m_src2Id(Metric::IData::npos),
      m_numSrc(0)
  { }

  AExprIncr(uint dstId, uint dst2Id, uint srcId)
    : m_dstId(dstId), m_dst2Id(dst2Id),
      m_srcId(srcId), m_src2Id(Metric::IData::npos),
      m_numSrc(0)
  { }

  virtual ~AExprIncr()
  { }


  void
  dstId(uint x)
  { m_dstId = x; }


  uint
  dst2Id()
  { return m_dst2Id; }

  void
  dst2Id(uint x)
  { m_dst2Id = x; }

  bool
  hasDst2Id() const
  { return (m_dst2Id != Metric::IData::npos); }


  void
  srcId(uint x)
  { m_srcId = x; }

  void
  src2Id(uint x)
  { m_src2Id = x; }

  bool
  hasSrc2Id() const
  { return (m_src2Id != Metric::IData::npos); }


  uint
  numSrc() const
  { return m_numSrc; }

  void
  numSrc(uint x)
  { m_numSrc = x; }


  // ------------------------------------------------------------
  //
  // ------------------------------------------------------------

  enum FnTy { FnInit, FnInitSrc, FnUpdate, FnCombine, FnFini };

  // initialize: initializes destination metrics (dstVar() & dst2Var())
  virtual double
  initialize(Metric::IData& mdata) const = 0;

  // initializeSrc: initializes source metrics (srcVar() & srcVar2())
  virtual double
  initializeSrc(Metric::IData& mdata) const = 0;

  // update: updates destination metrics using an individual source, srcVar().
  virtual double
  update(Metric::IData& mdata) const = 0;

  // combine: combines destination metrics with sources that themselves
  // represent destination metrics, i.e., they are the result of
  // updates ((srcVar() & srcVar2()).
  virtual double
  combine(Metric::IData& mdata) const = 0;

  // finalize: finalizes destination metrics using numSrc()
  virtual double
  finalize(Metric::IData& mdata) const = 0;


  // ------------------------------------------------------------
  // these correspond to AExpr::Var
  // ------------------------------------------------------------

  static double&
  var(Metric::IData& mdata, uint mId)
  { return mdata.demandMetric(mId); }

  static double
  var(const Metric::IData& mdata, uint mId)
  { return mdata.demandMetric(mId); }


  double
  dstVar(const Metric::IData& mdata) const
  { return var(mdata, m_dstId); }

  double&
  dstVar(Metric::IData& mdata) const
  { return var(mdata, m_dstId); }

  std::string
  dstStr() const
  { return "$"+ StrUtil::toStr(m_dstId); }


  double
  dst2Var(const Metric::IData& mdata) const
  { return var(mdata, m_dst2Id); }

  double&
  dst2Var(Metric::IData& mdata) const
  { return var(mdata, m_dst2Id); }

  std::string
  dst2Str() const
  { return "$"+ StrUtil::toStr(m_dst2Id); }


  double
  srcVar(const Metric::IData& mdata) const
  { return var(mdata, m_srcId); }

  double&
  srcVar(Metric::IData& mdata) const
  { return var(mdata, m_srcId); }

  std::string
  srcStr() const
  { return "$"+ StrUtil::toStr(m_srcId); }


  double
  src2Var(const Metric::IData& mdata) const
  { return var(mdata, m_src2Id); }

  double&
  src2Var(Metric::IData& mdata) const
  { return var(mdata, m_src2Id); }

  std::string
  src2Str() const
  { return "$"+ StrUtil::toStr(m_src2Id); }


  std::string
  numSrcStr() const
  { return StrUtil::toStr(m_numSrc); }


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
    dstVar(mdata) = 0.0;
    dst2Var(mdata) = 0.0;
    return 0.0;
  }


  double
  initializeSrcStdDev(Metric::IData& mdata) const
  {
    srcVar(mdata) = 0.0;
    if (hasSrc2Id()) {
      src2Var(mdata) = 0.0;
    }
    return 0.0;
  }


  double
  updateStdDev(Metric::IData& mdata) const
  {
    double d1 = dstVar(mdata), d2 = dst2Var(mdata), s = srcVar(mdata);
    double z1 = d1 + s;       // running sum
    double z2 = d2 + (s * s); // running sum of squares
    dstVar(mdata)  = z1;
    dst2Var(mdata) = z2;
    return z1;
  }


  double
  combineStdDev(Metric::IData& mdata) const
  {
    double d1 = dstVar(mdata), d2 = dst2Var(mdata);
    double s1 = srcVar(mdata), s2 = src2Var(mdata);
    double z1 = d1 + s1; // running sum
    double z2 = d2 + s2; // running sum of squares
    dstVar(mdata)  = z1;
    dst2Var(mdata) = z2;
    return z1;
  }


  std::string
  combineString1StdDev() const
  {
    std::string d1 = dstStr();
    std::string z1 = "sum(" + d1 + ", " + d1 + ")"; // running sum
    return z1;
  }

  std::string
  combineString2StdDev() const
  {
    std::string d2 = dst2Str();
    std::string z2 = "sum(" + d2 + ", " + d2 + ")"; // running sum of squares
    return z2;
  }


  double
  finalizeStdDev(Metric::IData& mdata) const
  {
    double d1 = dstVar(mdata), d2 = dst2Var(mdata);
    double sdev = d1;
    if (numSrc() > 0) {
      double n = numSrc();
      double mean = d1 / n;
      double z1 = (mean * mean); // (mean)^2
      double z2 = d2 / n;        // (sum of squares)/n
      sdev = sqrt(z2 - z1);      // stddev

      dstVar(mdata) = sdev;
      dst2Var(mdata) = mean;
    }
    return sdev;
  }


  std::string
  finalizeStringStdDev(std::string* meanRet = NULL) const
  {
    std::string n = numSrcStr();
    std::string d1 = dstStr(), d2 = dst2Str();

    std::string mean = "(" + d1 + " / " + n + ")";
    std::string z1 = "(" + mean + " * " + mean  + ")"; // (mean)^2
    std::string z2 = "(" + d2 + " / " + n  + ")";      // (sum of squares)/n
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
  uint m_dstId;
  uint m_dst2Id;
  uint m_srcId;
  uint m_src2Id;
  uint m_numSrc;
};


// ----------------------------------------------------------------------
// MinIncr
// ----------------------------------------------------------------------

class MinIncr
  : public AExprIncr
{
public:
  MinIncr(uint dstId, uint srcId)
    : AExprIncr(dstId, srcId)
  { }

  virtual ~MinIncr()
  { }


  virtual double
  initialize(Metric::IData& mdata) const
  { return (dstVar(mdata) = DBL_MAX); }

  virtual double
  initializeSrc(Metric::IData& mdata) const
  { return (srcVar(mdata) = DBL_MAX); }

  virtual double
  update(Metric::IData& mdata) const
  {
    double d = dstVar(mdata), s = srcVar(mdata);
    double z = std::min(d, s);
    DIAG_MsgIf(0, "MinIncr: min("<< d << ", " << s << ") = " << z);
    dstVar(mdata) = z;
    return z;
  }

  virtual double
  combine(Metric::IData& mdata) const
  { return MinIncr::update(mdata); }

  virtual double
  finalize(Metric::IData& mdata) const
  { return dstVar(mdata); }


  virtual std::string
  combineString1() const
  {
    std::string d = dstStr();
    std::string z = "min(" + d + ", " + d + ")";
    return z;
  }

  virtual std::string
  finalizeString() const
  { return dstStr(); }

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
  MaxIncr(uint dstId, uint srcId)
    : AExprIncr(dstId, srcId)
  { }

  virtual ~MaxIncr()
  { }


  virtual double
  initialize(Metric::IData& mdata) const
  { return (dstVar(mdata) = 0.0); }

  virtual double
  initializeSrc(Metric::IData& mdata) const
  { return (srcVar(mdata) = 0.0); }

  virtual double
  update(Metric::IData& mdata) const
  {
    double d = dstVar(mdata), s = srcVar(mdata);
    double z = std::max(d, s);
    DIAG_MsgIf(0, "MaxIncr: max("<< d << ", " << s << ") = " << z);
    dstVar(mdata) = z;
    return z;
  }

  virtual double
  combine(Metric::IData& mdata) const
  { return MaxIncr::update(mdata); }

  virtual double
  finalize(Metric::IData& mdata) const
  { return dstVar(mdata); }


  virtual std::string
  combineString1() const
  {
    std::string d = dstStr();
    std::string z = "max(" + d + ", " + d + ")";
    return z;
  }

  virtual std::string
  finalizeString() const
  { return dstStr(); }

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
  SumIncr(uint dstId, uint srcId)
    : AExprIncr(dstId, srcId)
  { }

  virtual ~SumIncr()
  { }


  virtual double
  initialize(Metric::IData& mdata) const
  { return (dstVar(mdata) = 0.0); }

  virtual double
  initializeSrc(Metric::IData& mdata) const
  { return (srcVar(mdata) = 0.0); }

  virtual double
  update(Metric::IData& mdata) const
  {
    double d = dstVar(mdata), s = srcVar(mdata);
    double z = d + s;
    DIAG_MsgIf(0, "SumIncr: +("<< d << ", " << s << ") = " << z);
    dstVar(mdata) = z;
    return z;
  }

  virtual double
  combine(Metric::IData& mdata) const
  { return SumIncr::update(mdata); }

  virtual double
  finalize(Metric::IData& mdata) const
  { return dstVar(mdata); }


  virtual std::string
  combineString1() const
  {
    std::string d = dstStr();
    std::string z = "sum(" + d + ", " + d + ")";
    return z;
  }

  virtual std::string
  finalizeString() const
  { return dstStr(); }

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
  MeanIncr(uint dstId, uint srcId)
    : AExprIncr(dstId, srcId)
  { }

  virtual ~MeanIncr()
  { }


  virtual double
  initialize(Metric::IData& mdata) const
  { return (dstVar(mdata) = 0.0); }

  virtual double
  initializeSrc(Metric::IData& mdata) const
  { return (srcVar(mdata) = 0.0); }

  virtual double
  update(Metric::IData& mdata) const
  {
    double d = dstVar(mdata), s = srcVar(mdata);
    double z = d + s;
    DIAG_MsgIf(0, "MeanIncr: +("<< d << ", " << s << ") = " << z);
    dstVar(mdata) = z;
    return z;
  }

  virtual double
  combine(Metric::IData& mdata) const
  { return MeanIncr::update(mdata); }

  virtual double
  finalize(Metric::IData& mdata) const
  {
    double d = dstVar(mdata);
    double z = d;
    if (numSrc() > 0) {
      double n = numSrc();
      z = d / n;
      dstVar(mdata) = z;
    }
    return z;
  }


  virtual std::string
  combineString1() const
  {
    std::string d = dstStr();
    std::string z = "sum(" + d + ", " + d + ")";
    return z;
  }

  virtual std::string
  finalizeString() const
  {
    std::string n = numSrcStr();
    std::string d = dstStr();
    std::string z = d + " / " + n;
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
  StdDevIncr(uint dstId, uint dst2Id, uint srcId)
    : AExprIncr(dstId, dst2Id, srcId)
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
  update(Metric::IData& mdata) const
  { return updateStdDev(mdata); }

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
  CoefVarIncr(uint dstId, uint dst2Id, uint srcId)
    : AExprIncr(dstId, dst2Id, srcId)
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
  update(Metric::IData& mdata) const
  { return updateStdDev(mdata); }

  virtual double
  combine(Metric::IData& mdata) const
  { return combineStdDev(mdata); }

  virtual double
  finalize(Metric::IData& mdata) const
  {
    double sdev = finalizeStdDev(mdata);
    double mean = dst2Var(mdata);
    double z = 0.0;
    if (mean > epsilon) {
      z = sdev / mean;
    }
    dstVar(mdata) = z;
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
  RStdDevIncr(uint dstId, uint dst2Id, uint srcId)
    : AExprIncr(dstId, dst2Id, srcId)
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
  update(Metric::IData& mdata) const
  { return updateStdDev(mdata); }

  virtual double
  combine(Metric::IData& mdata) const
  { return combineStdDev(mdata); }

  virtual double
  finalize(Metric::IData& mdata) const
  {
    double sdev = finalizeStdDev(mdata);
    double mean = dst2Var(mdata);
    double z = 0.0;
    if (mean > epsilon) {
      z = (sdev / mean) * 100;
    }
    dstVar(mdata) = z;
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
