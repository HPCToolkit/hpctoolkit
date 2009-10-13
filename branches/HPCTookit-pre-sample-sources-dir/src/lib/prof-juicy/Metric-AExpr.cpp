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

// ----------------------------------------------------------------------
//
//   the implementation of evaluation expressions
//
// ----------------------------------------------------------------------

//************************ System Include Files ******************************

#include <iostream> 
using std::endl;

#include <sstream>
#include <string>
#include <algorithm>

#include <cmath>

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
AExpr::dump_opands(std::ostream& os, AExpr** opands, int sz, const char* sep)
{
  for (int i = 0; i < sz; ++i) {
    opands[i]->dump(os);
    if (i < (sz - 1)) {
      os << sep;
    }
  }
}


// ----------------------------------------------------------------------
// class Const
// ----------------------------------------------------------------------

std::ostream& 
Const::dump(std::ostream& os) const
{ 
  os << "(" << m_c << ")"; 
  return os;
}


// ----------------------------------------------------------------------
// class Neg
// ----------------------------------------------------------------------

double 
Neg::eval(const Metric::IData& mdata) const
{
  double result = m_expr->eval(mdata);

  AEXPR_CHECK(result);
  return -result;
}


std::ostream& 
Neg::dump(std::ostream& os) const
{ 
  os << "(-";
  m_expr->dump(os);
  os << ")"; 
  return os;
}


// ----------------------------------------------------------------------
// class Var
// ----------------------------------------------------------------------

std::ostream& 
Var::dump(std::ostream& os) const
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
  double result = pow(b, e);
  
  AEXPR_CHECK(result);
  return result;
}


std::ostream& 
Power::dump(std::ostream& os) const
{
  os << "(";
  m_base->dump(os);
  os << "**";
  m_exponent->dump(os);
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
  double result = n / d;

  AEXPR_CHECK(result);
  return result;
}


std::ostream& 
Divide::dump(std::ostream& os) const
{
  os << "("; 
  m_numerator->dump(os); 
  os << " / "; 
  m_denominator->dump(os); 
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
  double result = (m - s);
  
  AEXPR_CHECK(result);
  return result;
}


std::ostream& 
Minus::dump(std::ostream& os) const
{
  os << "(";
  m_minuend->dump(os);
  os << " - ";
  m_subtrahend->dump(os); os << ")";
  return os;
}


// ----------------------------------------------------------------------
// class Plus
// ----------------------------------------------------------------------

Plus::~Plus() 
{
  for (int i = 0; i < m_sz; ++i) {
    delete m_opands[i];
  }
  delete[] m_opands;
}


double 
Plus::eval(const Metric::IData& mdata) const
{
  double result = evalSum(mdata, m_opands, m_sz);

  AEXPR_CHECK(result);
  return result;
}


std::ostream& 
Plus::dump(std::ostream& os) const
{
  os << "(";
  dump_opands(os, m_opands, m_sz, " + ");
  os << ")";
  return os;
}


// ----------------------------------------------------------------------
// class Times
// ----------------------------------------------------------------------

Times::~Times() 
{
  for (int i = 0; i < m_sz; ++i) {
    delete m_opands[i];
  }
  delete[] m_opands;
}


double 
Times::eval(const Metric::IData& mdata) const
{
  double result = 1.0;
  for (int i = 0; i < m_sz; ++i) {
    double x = m_opands[i]->eval(mdata);
    result *= x;
  }

  AEXPR_CHECK(result);
  return result;
}


std::ostream& 
Times::dump(std::ostream& os) const
{
  os << "(";
  dump_opands(os, m_opands, m_sz, " * ");
  os << ")";
  return os;
}


// ----------------------------------------------------------------------
// class Max
// ----------------------------------------------------------------------

Max::~Max() 
{
  for (int i = 0; i < m_sz; ++i) {
    delete m_opands[i];
  }
  delete[] m_opands;
}


double 
Max::eval(const Metric::IData& mdata) const
{
  double result = m_opands[0]->eval(mdata);
  for (int i = 1; i < m_sz; ++i) {
    double x = m_opands[i]->eval(mdata);
    result = std::max(result, x);
  }

  AEXPR_CHECK(result);
  return result;
}


std::ostream& 
Max::dump(std::ostream& os) const
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
  for (int i = 0; i < m_sz; ++i) {
    delete m_opands[i];
  }
  delete[] m_opands;
}


double 
Min::eval(const Metric::IData& mdata) const
{
  double result = m_opands[0]->eval(mdata);
  for (int i = 1; i < m_sz; ++i) {
    double x = m_opands[i]->eval(mdata);
    result = std::min(result, x);
  }

  AEXPR_CHECK(result);
  return result;
}


std::ostream& 
Min::dump(std::ostream& os) const
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
  for (int i = 0; i < m_sz; ++i) {
    delete m_opands[i];
  }
  delete[] m_opands;
}


double 
Mean::eval(const Metric::IData& mdata) const
{
  double result = evalMean(mdata, m_opands, m_sz);

  AEXPR_CHECK(result);
  return result;
}


std::ostream& 
Mean::dump(std::ostream& os) const
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
  for (int i = 0; i < m_sz; ++i) {
    delete m_opands[i];
  }
  delete[] m_opands;
}


double 
StdDev::eval(const Metric::IData& mdata) const
{
  std::pair<double, double> v_m = evalVariance(mdata, m_opands, m_sz);
  double result = sqrt(v_m.first);

  AEXPR_CHECK(result);
  return result;
}


std::ostream& 
StdDev::dump(std::ostream& os) const
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
  for (int i = 0; i < m_sz; ++i) {
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
  double result = 0.0;
  if (mean > epsilon) {
    result = sdev / mean;
  }

  AEXPR_CHECK(result);
  return result;
}


std::ostream& 
CoefVar::dump(std::ostream& os) const
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
  for (int i = 0; i < m_sz; ++i) {
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
  double result = 0.0;
  if (mean > epsilon) {
    result = (sdev / mean) * 100;
  }

  AEXPR_CHECK(result);
  return result;
}


std::ostream& 
RStdDev::dump(std::ostream& os) const
{
  os << "r-stddev(";
  dump_opands(os, m_opands, m_sz);
  os << ")";
  return os;
}

//****************************************************************************


} // namespace Metric

} // namespace Prof
