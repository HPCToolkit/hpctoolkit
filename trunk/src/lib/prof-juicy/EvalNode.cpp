// -*-Mode: C++;-*-
// $Id$

// * BeginRiceCopyright *****************************************************
// 
// Copyright ((c)) 2002-2007, Rice University 
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
//   the implementation of evaluation tree nodes
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

#include <include/general.h>

#include "EvalNode.hpp"

#include <lib/support/NaN.h>
#include <lib/support/Trace.hpp>

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
Neg::eval(const ScopeInfo* si) const
{
  double result = m_expr->eval(si);

  //IFTRACE << "neg=" << -result << endl; 
  AEXPR_CHECK(result);
  return -result;
}


std::ostream& 
Neg::dump(std::ostream& os) const
{ 
  os << "(-"; m_expr->dump(os); os << ")"; 
  return os;
}


// ----------------------------------------------------------------------
// class Var
// ----------------------------------------------------------------------

std::ostream& 
Var::dump(std::ostream& os) const
{ 
  os << name; 
  return os;
}


// ----------------------------------------------------------------------
// class Power
// ----------------------------------------------------------------------

Power::Power(AExpr* b, AExpr* e) 
  : base(b), exponent(e) 
{ 
}


Power::~Power() 
{ 
  delete base; 
  delete exponent; 
}


double 
Power::eval(const ScopeInfo* si) const
{
  double b = base->eval(si);
  double e = exponent->eval(si);
  double result = pow(b, e);
  
  //IFTRACE << "pow=" << pow(b, e) << endl; 
  AEXPR_CHECK(result);
  return result;
}


std::ostream& 
Power::dump(std::ostream& os) const
{
  os << "("; base->dump(os); os << "**"; exponent->dump(os); os << ")";
  return os;
}


// ----------------------------------------------------------------------
// class Divide
// ----------------------------------------------------------------------

Divide::Divide(AExpr* num, AExpr* denom)
  : numerator(num), denominator(denom) 
{ 
}


Divide::~Divide() 
{ 
  delete numerator; 
  delete denominator; 
}


double 
Divide::eval(const ScopeInfo* si) const
{
  double n = numerator->eval(si);
  double d = denominator->eval(si);
  double result = n / d;

  //IFTRACE << "divident=" << n/d << endl; 
  AEXPR_CHECK(result);
  return result;
}


std::ostream& 
Divide::dump(std::ostream& os) const
{
  os << "("; 
  numerator->dump(os); 
  os << " / "; 
  denominator->dump(os); 
  os << ")";
  return os;
}


// ----------------------------------------------------------------------
// class Minus
// ----------------------------------------------------------------------

Minus::Minus(AExpr* m, AExpr* s) 
  : minuend(m), subtrahend(s) 
{ 
  // if (minuend == NULL || subtrahend == NULL) { ... }
}


Minus::~Minus() 
{ 
  delete minuend; 
  delete subtrahend; 
}


double 
Minus::eval(const ScopeInfo* si) const
{
  double m = minuend->eval(si);
  double s = subtrahend->eval(si);
  double result = (m - s);
  
  //IFTRACE << "diff=" << m-s << endl; 
  AEXPR_CHECK(result);
  return result;
}


std::ostream& 
Minus::dump(std::ostream& os) const
{
  os << "("; minuend->dump(os); os << " - "; subtrahend->dump(os); os << ")";
  return os;
}


// ----------------------------------------------------------------------
// class Plus
// ----------------------------------------------------------------------

Plus::Plus(AExpr** oprnds, int numOprnds)
  : m_opands(oprnds), m_sz(numOprnds) 
{ 
}


Plus::~Plus() 
{
  for (int i = 0; i < m_sz; ++i) {
    delete m_opands[i];
  }
  delete[] m_opands;
}


double 
Plus::eval(const ScopeInfo* si) const
{
  double result = eval_sum(si, m_opands, m_sz);

  //IFTRACE << "sum=" << result << endl; 
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

Times::Times(AExpr** oprnds, int numOprnds) 
  : m_opands(oprnds), m_sz(numOprnds) 
{ 
}


Times::~Times() 
{
  for (int i = 0; i < m_sz; ++i) {
    delete m_opands[i];
  }
  delete[] m_opands;
}


double 
Times::eval(const ScopeInfo* si) const
{
  double result = 1.0;
  for (int i = 0; i < m_sz; ++i) {
    double x = m_opands[i]->eval(si);
    result *= x;
  }
  //IFTRACE << "result=" << result << endl; 
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

Max::Max(AExpr** oprnds, int numOprnds) 
  : m_opands(oprnds), m_sz(numOprnds) 
{ 
}


Max::~Max() 
{
  for (int i = 0; i < m_sz; ++i) {
    delete m_opands[i];
  }
  delete[] m_opands;
}


double 
Max::eval(const ScopeInfo* si) const
{
  double result = m_opands[0]->eval(si);
  for (int i = 1; i < m_sz; ++i) {
    double x = m_opands[i]->eval(si);
    result = std::max(result, x);
  }

  //IFTRACE << "max=" << result << endl; 
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

Min::Min(AExpr** oprnds, int numOprnds) 
  : m_opands(oprnds), m_sz(numOprnds) 
{ 
}


Min::~Min() 
{
  for (int i = 0; i < m_sz; ++i) {
    delete m_opands[i];
  }
  delete[] m_opands;
}


double 
Min::eval(const ScopeInfo* si) const
{
  double result = m_opands[0]->eval(si);
  for (int i = 1; i < m_sz; ++i) {
    double x = m_opands[i]->eval(si);
    result = std::min(result, x);
  }

  //IFTRACE << "min=" << result << endl;   
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

Mean::Mean(AExpr** oprnds, int numOprnds)
  : m_opands(oprnds), m_sz(numOprnds) 
{ 
}


Mean::~Mean() 
{
  for (int i = 0; i < m_sz; ++i) {
    delete m_opands[i];
  }
  delete[] m_opands;
}


double 
Mean::eval(const ScopeInfo* si) const
{
  double result = eval_mean(si, m_opands, m_sz);

  //IFTRACE << "mean=" << result << endl; 
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

StdDev::StdDev(AExpr** oprnds, int numOprnds)
  : m_opands(oprnds), m_sz(numOprnds) 
{ 
}


StdDev::~StdDev() 
{
  for (int i = 0; i < m_sz; ++i) {
    delete m_opands[i];
  }
  delete[] m_opands;
}


double 
StdDev::eval(const ScopeInfo* si) const
{
  double* x = new double[m_sz];

  double x_mean = 0.0; // mean
  for (int i = 0; i < m_sz; ++i) {
    double t = m_opands[i]->eval(si);
    x[i] = t;
    x_mean += t;
  }
  x_mean = x_mean / m_sz;

  double x_var = 0.0; // variance
  for (int i = 0; i < m_sz; ++i) {
    double t = (x[i] - x_mean);
    t = t * t;
    x_var += t;
  }
  x_var = x_var / m_sz;

  double result = sqrt(x_var);
  delete[] x;

  //IFTRACE << "stddev=" << result << endl; 
  AEXPR_CHECK(result);
  return result;
}


std::ostream& 
StdDev::dump(std::ostream& os) const
{
  os << "sdev(";
  dump_opands(os, m_opands, m_sz);
  os << ")";
  return os;
}

//****************************************************************************


} // namespace Metric

} // namespace Prof
