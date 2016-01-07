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
//  Prof::Metric::AExpr and derived classes
//
//***************************************************************************

//************************ System Include Files ******************************

#include <iostream>
using std::endl;

#include <sstream>
#include <string>
#include <algorithm>

#include <cmath>
#include <cfloat>

//************************* User Include Files *******************************

#include <include/uint.h>

#include "Metric-AExpr.hpp"

#include <lib/support/diagnostics.h>
#include <lib/support/NaN.h>

//************************ Forward Declarations ******************************

#define AEXPR_DO_CHECK 0
#if (AEXPR_DO_CHECK)
# define AEXPR_CHECK(x) if (!isok(x)) { return c_FP_NAN_d; }
#else
# define AEXPR_CHECK(x)
#endif


//****************************************************************************

namespace Prof {

namespace Metric {


// ----------------------------------------------------------------------
// class AExpr
// ----------------------------------------------------------------------

std::string
AExpr::toString() const
{
  std::ostringstream os;
  dump(os);
  return os.str();
}


void
AExpr::dump_opands(std::ostream& os, AExpr** opands, uint sz, const char* sep)
{
  for (uint i = 0; i < sz; ++i) {
    opands[i]->dumpMe(os);
    if (i < (sz - 1)) {
      os << sep;
    }
  }
}


// ----------------------------------------------------------------------
// class Const
// ----------------------------------------------------------------------

std::ostream&
Const::dumpMe(std::ostream& os) const
{
  os << m_c;
  return os;
}


// ----------------------------------------------------------------------
// class Neg
// ----------------------------------------------------------------------

double
Neg::eval(const Metric::IData& mdata) const
{
  double z = m_expr->eval(mdata);

  AEXPR_CHECK(z);
  return -z;
}


std::ostream&
Neg::dumpMe(std::ostream& os) const
{
  os << "-(";
  m_expr->dumpMe(os);
  os << ")";
  return os;
}


// ----------------------------------------------------------------------
// class Var
// ----------------------------------------------------------------------

std::ostream&
Var::dumpMe(std::ostream& os) const
{
  os << m_name;
  return os;
}


// ----------------------------------------------------------------------
// class Power
// ----------------------------------------------------------------------

double
Power::eval(const Metric::IData& mdata) const
{
  double b = m_base->eval(mdata);
  double e = m_exponent->eval(mdata);
  double z = pow(b, e);

  AEXPR_CHECK(z);
  return z;
}


std::ostream&
Power::dumpMe(std::ostream& os) const
{
  os << "(";
  m_base->dumpMe(os);
  os << "**";
  m_exponent->dumpMe(os);
  os << ")";
  return os;
}


// ----------------------------------------------------------------------
// class Divide
// ----------------------------------------------------------------------

double
Divide::eval(const Metric::IData& mdata) const
{
  double n = m_numerator->eval(mdata);
  double d = m_denominator->eval(mdata);

  double z = c_FP_NAN_d;
  if (isok(d) && d != 0.0) {
    z = n / d;
  }

  AEXPR_CHECK(z);
  return z;
}


std::ostream&
Divide::dumpMe(std::ostream& os) const
{
  os << "(";
  m_numerator->dumpMe(os);
  os << " / ";
  m_denominator->dumpMe(os);
  os << ")";
  return os;
}


// ----------------------------------------------------------------------
// class Minus
// ----------------------------------------------------------------------

double
Minus::eval(const Metric::IData& mdata) const
{
  double m = m_minuend->eval(mdata);
  double s = m_subtrahend->eval(mdata);
  double z = (m - s);

  AEXPR_CHECK(z);
  return z;
}


std::ostream&
Minus::dumpMe(std::ostream& os) const
{
  os << "(";
  m_minuend->dumpMe(os);
  os << " - ";
  m_subtrahend->dumpMe(os); os << ")";
  return os;
}


// ----------------------------------------------------------------------
// class Plus
// ----------------------------------------------------------------------

Plus::~Plus()
{
  for (uint i = 0; i < m_sz; ++i) {
    delete m_opands[i];
  }
  delete[] m_opands;
}


double
Plus::eval(const Metric::IData& mdata) const
{
  double z = evalSum(mdata, m_opands, m_sz);

  AEXPR_CHECK(z);
  return z;
}


std::ostream&
Plus::dumpMe(std::ostream& os) const
{
  if (m_sz > 1) { os << "("; }
  dump_opands(os, m_opands, m_sz, " + ");
  if (m_sz > 1) { os << ")"; }
  return os;
}


// ----------------------------------------------------------------------
// class Times
// ----------------------------------------------------------------------

Times::~Times()
{
  for (uint i = 0; i < m_sz; ++i) {
    delete m_opands[i];
  }
  delete[] m_opands;
}


double
Times::eval(const Metric::IData& mdata) const
{
  double z = 1.0;
  for (uint i = 0; i < m_sz; ++i) {
    double x = m_opands[i]->eval(mdata);
    z *= x;
  }

  AEXPR_CHECK(z);
  return z;
}


std::ostream&
Times::dumpMe(std::ostream& os) const
{
  if (m_sz > 1) { os << "("; }
  dump_opands(os, m_opands, m_sz, " * ");
  if (m_sz > 1) { os << ")"; }
  return os;
}


// ----------------------------------------------------------------------
// class Max
// ----------------------------------------------------------------------

Max::~Max()
{
  for (uint i = 0; i < m_sz; ++i) {
    delete m_opands[i];
  }
  delete[] m_opands;
}


double
Max::eval(const Metric::IData& mdata) const
{
  double z = m_opands[0]->eval(mdata);
  for (uint i = 1; i < m_sz; ++i) {
    double x = m_opands[i]->eval(mdata);
    z = std::max(z, x);
  }

  AEXPR_CHECK(z);
  return z;
}


std::ostream&
Max::dumpMe(std::ostream& os) const
{
  os << "max(";
  dump_opands(os, m_opands, m_sz);
  os << ")";
  return os;
}


// ----------------------------------------------------------------------
// class Min
// ----------------------------------------------------------------------

Min::~Min()
{
  for (uint i = 0; i < m_sz; ++i) {
    delete m_opands[i];
  }
  delete[] m_opands;
}


double
Min::eval(const Metric::IData& mdata) const
{
  double z = DBL_MAX;
  for (uint i = 0; i < m_sz; ++i) {
    double x = m_opands[i]->eval(mdata);
    if (x != 0.0) {
      z = std::min(z, x);
    }
  }

  if (z == DBL_MAX) { z = DBL_MIN; }

  AEXPR_CHECK(z);
  return z;
}


std::ostream&
Min::dumpMe(std::ostream& os) const
{
  os << "min(";
  dump_opands(os, m_opands, m_sz);
  os << ")";
  return os;
}


// ----------------------------------------------------------------------
// class Mean
// ----------------------------------------------------------------------

Mean::~Mean()
{
  for (uint i = 0; i < m_sz; ++i) {
    delete m_opands[i];
  }
  delete[] m_opands;
}


double
Mean::eval(const Metric::IData& mdata) const
{
  double z = evalMean(mdata, m_opands, m_sz);

  AEXPR_CHECK(z);
  return z;
}


std::ostream&
Mean::dumpMe(std::ostream& os) const
{
  os << "mean(";
  dump_opands(os, m_opands, m_sz);
  os << ")";
  return os;
}


// ----------------------------------------------------------------------
// class StdDev
// ----------------------------------------------------------------------

StdDev::~StdDev()
{
  for (uint i = 0; i < m_sz; ++i) {
    delete m_opands[i];
  }
  delete[] m_opands;
}


double
StdDev::eval(const Metric::IData& mdata) const
{
  std::pair<double, double> v_m = evalVariance(mdata, m_opands, m_sz);
  double z = sqrt(v_m.first);

  AEXPR_CHECK(z);
  return z;
}


std::ostream&
StdDev::dumpMe(std::ostream& os) const
{
  os << "stddev(";
  dump_opands(os, m_opands, m_sz);
  os << ")";
  return os;
}


// ----------------------------------------------------------------------
// class CoefVar
// ----------------------------------------------------------------------

CoefVar::~CoefVar()
{
  for (uint i = 0; i < m_sz; ++i) {
    delete m_opands[i];
  }
  delete[] m_opands;
}


double
CoefVar::eval(const Metric::IData& mdata) const
{
  std::pair<double, double> v_m = evalVariance(mdata, m_opands, m_sz);
  double sdev = sqrt(v_m.first); // always non-negative
  double mean = v_m.second;
  double z = 0.0;
  if (mean > epsilon) {
    z = sdev / mean;
  }

  AEXPR_CHECK(z);
  return z;
}


std::ostream&
CoefVar::dumpMe(std::ostream& os) const
{
  os << "coefvar(";
  dump_opands(os, m_opands, m_sz);
  os << ")";
  return os;
}


// ----------------------------------------------------------------------
// class RStdDev
// ----------------------------------------------------------------------

RStdDev::~RStdDev()
{
  for (uint i = 0; i < m_sz; ++i) {
    delete m_opands[i];
  }
  delete[] m_opands;
}


double
RStdDev::eval(const Metric::IData& mdata) const
{
  std::pair<double, double> v_m = evalVariance(mdata, m_opands, m_sz);
  double sdev = sqrt(v_m.first); // always non-negative
  double mean = v_m.second;
  double z = 0.0;
  if (mean > epsilon) {
    z = (sdev / mean) * 100;
  }

  AEXPR_CHECK(z);
  return z;
}


std::ostream&
RStdDev::dumpMe(std::ostream& os) const
{
  os << "r-stddev(";
  dump_opands(os, m_opands, m_sz);
  os << ")";
  return os;
}


// ----------------------------------------------------------------------
// class NumSource
// ----------------------------------------------------------------------

std::ostream&
NumSource::dumpMe(std::ostream& os) const
{
  os << "num-src()";
  return os;
}


//****************************************************************************


} // namespace Metric

} // namespace Prof
