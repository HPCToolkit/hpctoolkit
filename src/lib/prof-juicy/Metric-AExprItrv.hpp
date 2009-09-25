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
// Currently supported expressions are
//   Max    : max expression                       : n-ary
//   Min    : min expression                       : n-ary
//   Mean   : mean (arithmetic) expression         : n-ary
//   StdDev : standard deviation expression        : n-ary
//   CoefVar: coefficient of variance              : n-ary
//   RStdDev: relative standard deviation          : n-ary
//
//***************************************************************************

#ifndef prof_juicy_Prof_Metric_AExprItrv_hpp
#define prof_juicy_Prof_Metric_AExprItrv_hpp

//************************ System Include Files ******************************

#include <iostream> 
#include <string>

#include <cfloat>

//************************* User Include Files *******************************

#include "Metric-IData.hpp"

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
    : m_dstId(dstId), m_srcId(srcId)
  { }

  virtual ~AExprItrv()
  { }

  void
  dstId(uint x)
  { m_dstId = x; }

  // ------------------------------------------------------------
  //
  // ------------------------------------------------------------

  virtual double
  initialize(Metric::IData& mdata) const = 0;

  virtual double
  update(Metric::IData& mdata, uint numSrc = 1) const = 0;

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


  double&
  dstVar(Metric::IData& mdata) const
  { return var(mdata, m_dstId); }

  double
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
  uint m_srcId;
};


// ----------------------------------------------------------------------
// MaxItrv
// ----------------------------------------------------------------------

class MaxItrv : public AExprItrv
{
public:
  MaxItrv(uint dstId, uint srcId)
    : AExprItrv(dstId, srcId)
  { }

  ~MaxItrv()
  { }


  virtual double
  initialize(Metric::IData& mdata) const
  { return (dstVar(mdata) = 0.0); }

  virtual double
  update(Metric::IData& mdata, uint numSrc = 1) const
  {
    double x = std::max(dstVar(mdata), srcVar(mdata));
    return (dstVar(mdata) = x);
  }

  virtual double
  finalize(Metric::IData& mdata, uint numSrc) const
  { return dstVar(mdata); }


  virtual std::ostream&
  dump_me(std::ostream& os = std::cout) const;

private:

};


#if 0
// ----------------------------------------------------------------------
// Min
// ----------------------------------------------------------------------

class Min : public AExprItrv
{
public:
  // Assumes ownership of AExprItrv
  Min(AExprItrv** oprnds, int numOprnds)
    : m_opands(oprnds), m_sz(numOprnds) 
  { }

  ~Min();

  double
  eval(const Metric::IData& mdata) const;

  std::ostream&
  dump(std::ostream& os = std::cout) const;

private:
  AExprItrv** m_opands;
  int m_sz;
};


// ----------------------------------------------------------------------
// Mean
// ----------------------------------------------------------------------

class Mean : public AExprItrv
{
public:
  // Assumes ownership of AExprItrv
  Mean(AExprItrv** oprnds, int numOprnds)
    : m_opands(oprnds), m_sz(numOprnds) 
  { }

  ~Mean();

  double
  eval(const Metric::IData& mdata) const;

  std::ostream&
  dump(std::ostream& os = std::cout) const;

private:
  AExprItrv** m_opands;
  int m_sz;
};


// ----------------------------------------------------------------------
// StdDev: standard deviation
// ----------------------------------------------------------------------

class StdDev : public AExprItrv
{
public:
  // Assumes ownership of AExprItrv
  StdDev(AExprItrv** oprnds, int numOprnds)
    : m_opands(oprnds), m_sz(numOprnds) 
  { }

  ~StdDev();

  double
  eval(const Metric::IData& mdata) const;

  std::ostream&
  dump(std::ostream& os = std::cout) const;

private:
  AExprItrv** m_opands;
  int m_sz;
};


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
