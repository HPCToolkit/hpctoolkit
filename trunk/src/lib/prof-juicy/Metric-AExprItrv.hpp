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
// An abstract expression that represents derived expressions that
// are iteratively computed (as opposed to directly computed).
// 
// Since all sources are *not* known in advance, it is necessary to
// use an 'accumulator' that is iteratively updated and serves as both
// an input and output on each update.  This implies that is is
// necesary, in general, to have an initialization routine so the
// accumulator is correctly initialized before the first update.
// Additionally, since some metrics rely on the total number of
// inputs, a finaliation routine is also necessary.
//
// Currently supported expressions are
//   Max    : max expression
//   Min    : min expression
//   Mean   : mean (arithmetic) expression
//   StdDev : standard deviation expression
//   CoefVar: coefficient of variance
//   RStdDev: relative standard deviation
//
//***************************************************************************

#ifndef prof_juicy_Prof_Metric_AExprItrv_hpp
#define prof_juicy_Prof_Metric_AExprItrv_hpp

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

//************************ Forward Declarations ******************************

//****************************************************************************

namespace Prof {

namespace Metric {

// ----------------------------------------------------------------------
// class AExprItrv
//   The base class for all concrete evaluation classes
// ----------------------------------------------------------------------

class AExprItrv
  : public Unique // disable copying, for now
{
public:
  AExprItrv(uint dstId, uint srcId)
    : m_dstId(dstId), m_dst2Id(Metric::IData::npos), m_srcId(srcId)
  { }

  AExprItrv(uint dstId, uint dst2Id, uint srcId)
    : m_dstId(dstId), m_dst2Id(dst2Id), m_srcId(srcId)
  { }

  virtual ~AExprItrv()
  { }

  void
  dstId(uint x)
  { m_dstId = x; }

  void
  dst2Id(uint x)
  { m_dst2Id = x; }

  void
  srcId(uint x)
  { m_srcId = x; }


  // ------------------------------------------------------------
  //
  // ------------------------------------------------------------

  enum FnTy { FnInit, FnInitSrc, FnUpdate, FnFini };

  // initializes 'dstId'
  virtual double
  initialize(Metric::IData& mdata) const = 0;

  // initializes 'srcId'
  virtual double
  initializeSrc(Metric::IData& mdata) const = 0;

  // updates 'dstId' using 'srdId'
  virtual double
  update(Metric::IData& mdata) const = 0;

  // finalizes 'dstId' give the total number of sources
  virtual double
  finalize(Metric::IData& mdata, uint numSrc) const = 0;


  // ------------------------------------------------------------
  // correspond to AExpr::Var
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


  double
  dst2Var(const Metric::IData& mdata) const
  { return var(mdata, m_dst2Id); }

  double&
  dst2Var(Metric::IData& mdata) const
  { return var(mdata, m_dst2Id); }


  double
  srcVar(const Metric::IData& mdata) const
  { return var(mdata, m_srcId); }

  double&
  srcVar(Metric::IData& mdata) const
  { return var(mdata, m_srcId); }


  // ------------------------------------------------------------
  //
  // ------------------------------------------------------------

  static bool
  isok(double x)
  { return !(c_isnan_d(x) || c_isinf_d(x)); }

  // ------------------------------------------------------------
  //
  // ------------------------------------------------------------
  
  virtual std::string
  toString() const;

  std::ostream&
  dump(std::ostream& os = std::cerr) const
  {
    dump_me(os);
    return os;
  }

  virtual std::ostream&
  dump_me(std::ostream& os = std::cout) const = 0;
  
  void
  ddump() const;

protected:
  uint m_dstId;
  uint m_dst2Id;
  uint m_srcId;
};


// ----------------------------------------------------------------------
// MaxItrv
// ----------------------------------------------------------------------

class MaxItrv
  : public AExprItrv
{
public:
  MaxItrv(uint dstId, uint srcId)
    : AExprItrv(dstId, srcId)
  { }

  virtual ~MaxItrv()
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
    DIAG_MsgIf(0, "MaxItrv: max("<< d << ", " << s << ") = " << z);
    dstVar(mdata) = z;
    return z;
  }

  virtual double
  finalize(Metric::IData& mdata, uint numSrc) const
  { return dstVar(mdata); }


  virtual std::ostream&
  dump_me(std::ostream& os = std::cout) const;

private:
};


// ----------------------------------------------------------------------
// MinItrv
// ----------------------------------------------------------------------

class MinItrv
  : public AExprItrv
{
public:
  MinItrv(uint dstId, uint srcId)
    : AExprItrv(dstId, srcId)
  { }

  virtual ~MinItrv()
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
    DIAG_MsgIf(0, "MinItrv: min("<< d << ", " << s << ") = " << z);
    dstVar(mdata) = z;
    return z;
  }

  virtual double
  finalize(Metric::IData& mdata, uint numSrc) const
  { return dstVar(mdata); }


  virtual std::ostream&
  dump_me(std::ostream& os = std::cout) const;

private:
};


// ----------------------------------------------------------------------
// MeanItrv
// ----------------------------------------------------------------------

class MeanItrv
  : public AExprItrv
{
public:
  MeanItrv(uint dstId, uint srcId)
    : AExprItrv(dstId, srcId)
  { }

  virtual ~MeanItrv()
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
    DIAG_MsgIf(0, "MeanItrv: +("<< d << ", " << s << ") = " << z);
    dstVar(mdata) = z;
    return z;
  }

  virtual double
  finalize(Metric::IData& mdata, uint numSrc) const
  {
    double d = dstVar(mdata);
    double z = d;
    if (numSrc > 0) {
      z = d / (double)numSrc;
      dstVar(mdata) = z;
    }
    return z;
  }


  virtual std::ostream&
  dump_me(std::ostream& os = std::cout) const;

private:
};


// ----------------------------------------------------------------------
// StdDevItrv: standard deviation
// ----------------------------------------------------------------------

class StdDevItrv 
  : public AExprItrv
{
public:
  StdDevItrv(uint dstId, uint dst2Id, uint srcId)
    : AExprItrv(dstId, dst2Id, srcId)
  { }

  virtual ~StdDevItrv()
  { }


  virtual double
  initialize(Metric::IData& mdata) const
  {
    dst2Var(mdata) = 0.0;
    return (dstVar(mdata) = 0.0);
  }

  virtual double
  initializeSrc(Metric::IData& mdata) const
  { return (srcVar(mdata) = 0.0); }

  virtual double
  update(Metric::IData& mdata) const
  {
    double d1 = dstVar(mdata), d2 = dst2Var(mdata), s = srcVar(mdata);
    double z1 = d1 + s;       // running sum
    double z2 = d2 + (s * s); // running sum of squares
    dstVar(mdata)  = z1;
    dst2Var(mdata) = z2;
    return z1;
  }

  virtual double
  finalize(Metric::IData& mdata, uint numSrc) const
  {
    double d1 = dstVar(mdata), d2 = dst2Var(mdata);
    double z = d1;
    if (numSrc > 0) {
      double mean = d1 / numSrc;
      double z1 = (mean * mean); // (mean)^2
      double z2 = d2 / numSrc;   // (sum of squares)/N
      z = sqrt(z2 - z1);         // stddev
      dstVar(mdata) = z;
    }
    return z;
  }


  virtual std::ostream&
  dump_me(std::ostream& os = std::cout) const;

private:
};


#if 0
// ----------------------------------------------------------------------
// CoefVar: relative standard deviation
// ----------------------------------------------------------------------

class CoefVar : public AExprItrv
{
public:
  // Assumes ownership of AExprItrv
  CoefVar(AExprItrv** oprnds, int numOprnds)
    : m_opands(oprnds), m_sz(numOprnds) 
  { }

  ~CoefVar();

  double
  eval(const Metric::IData& mdata) const;

  std::ostream&
  dump(std::ostream& os = std::cout) const;

private:
  AExprItrv** m_opands;
  int m_sz;
};


// ----------------------------------------------------------------------
// RStdDev: relative standard deviation
// ----------------------------------------------------------------------

class RStdDev : public AExprItrv
{
public:
  // Assumes ownership of AExprItrv
  RStdDev(AExprItrv** oprnds, int numOprnds)
    : m_opands(oprnds), m_sz(numOprnds) 
  { }

  ~RStdDev();

  double
  eval(const Metric::IData& mdata) const;

  std::ostream&
  dump(std::ostream& os = std::cout) const;

private:
  AExprItrv** m_opands;
  int m_sz;
};
#endif

//****************************************************************************

} // namespace Metric

} // namespace Prof

//****************************************************************************

#endif /* prof_juicy_Prof_Metric_AExprItrvItrv_hpp */
