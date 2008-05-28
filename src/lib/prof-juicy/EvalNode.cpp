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

#ifdef NO_STD_CHEADERS
# include <math.h>
#else
# include <cmath>
using namespace std; // For compatibility with non-std C headers
#endif

//************************* User Include Files *******************************

#include <include/general.h>

#include "EvalNode.hpp"

#include <lib/support/NaN.h>
#include <lib/support/Trace.hpp>

//************************ Forward Declarations ******************************

//****************************************************************************

// ----------------------------------------------------------------------
// class EvalNode
// ----------------------------------------------------------------------

std::string
EvalNode::toString() const
{
  std::ostringstream os;
  dump(os);
  return os.str();
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
Neg::eval(const ScopeInfo* si) 
{
  double tmp = m_expr->eval(si);
  if (c_isnan_d(tmp) || c_isinf_d(tmp)) {
    return c_FP_NAN_d;
  }

  //IFTRACE << "neg=" << -tmp << endl; 
  return -tmp;
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

Power::Power(EvalNode* b, EvalNode* e) 
  : base(b), exponent(e) 
{ 
}


Power::~Power() 
{ 
  delete base; 
  delete exponent; 
}


double 
Power::eval(const ScopeInfo* si) 
{
  if (base == NULL || exponent == NULL)
    return c_FP_NAN_d;
  double b = base->eval(si);
  double e = exponent->eval(si);
  if ((c_isnan_d(e) || c_isinf_d(e)) && (c_isnan_d(b) || c_isinf_d(b))) {
    // do not create a value if both are missing
    return c_FP_NAN_d;
  }
  if (c_isnan_d(e) || c_isinf_d(e) || e == 0) {
    // exp is missing, assume it is zero
    return 1;
  }
  else {
    if (c_isnan_d(b) || c_isinf_d(b) || b == 0) {
      return 0;
    }
  }
  // if b < 0, pow works only if e is integer
  // I will simplify. If b<0, return c_FP_NAN_d;
  if (b < 0) {
    return c_FP_NAN_d;
  }
  //IFTRACE << "pow=" << pow(b, e) << endl; 
  return pow(b, e);
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

Divide::Divide(EvalNode* num, EvalNode* denom)
  : numerator(num), denominator(denom) 
{ 
}


Divide::~Divide() 
{ 
  delete numerator; 
  delete denominator; 
}


double 
Divide::eval(const ScopeInfo* si) 
{
  if (numerator == NULL || denominator == NULL) {
    return c_FP_NAN_d;
  }
  double n = numerator->eval(si);
  double d = denominator->eval(si);
  if (c_isnan_d(d) || c_isinf_d(d) || d == 0.0) {
    return c_FP_NAN_d;
  }
  if (c_isnan_d(n) || c_isinf_d(n)) {
    return 0;
  }
  //IFTRACE << "divident=" << n/d << endl; 
  return n / d;
}


std::ostream& 
Divide::dump(std::ostream& os) const
{
  os << "("; 
  numerator->dump(os); 
  os << "/"; 
  denominator->dump(os); 
  os << ")";
  return os;
}


// ----------------------------------------------------------------------
// class Minus
// ----------------------------------------------------------------------

Minus::Minus(EvalNode* m, EvalNode* s) 
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
Minus::eval(const ScopeInfo* si) 
{
  double m = minuend->eval(si);
  double s = subtrahend->eval(si);
  if ((c_isnan_d(m) || c_isinf_d(m)) && (c_isnan_d(s) || c_isinf_d(s))) {
    return c_FP_NAN_d;
  }
  if (c_isnan_d(m) || c_isinf_d(m)) {
    m = 0.0;
  }
  if (c_isnan_d(s) || c_isinf_d(s)) {
    s = 0.0;
  }
  //IFTRACE << "diff=" << m-s << endl; 
  return (m - s);
}


std::ostream& 
Minus::dump(std::ostream& os) const
{
  os << "("; minuend->dump(os); os << "-"; subtrahend->dump(os); os << ")";
  return os;
}


// ----------------------------------------------------------------------
// class Plus
// ----------------------------------------------------------------------

Plus::Plus(EvalNode** oprnds, int numOprnds)
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
Plus::eval(const ScopeInfo* si) 
{
  double result = c_FP_NAN_d;

  int i;
  for (i = 0; i < m_sz; ++i) {
    if (m_opands[i]) {
      double tmp = m_opands[i]->eval(si);
      if (c_isnan_d(tmp) || c_isinf_d(tmp)) {
	continue; 
      }
      result = tmp;
      break;
    }
  }
  for (++i; i < m_sz; ++i) {
    if (m_opands[i]) {
      double tmp = m_opands[i]->eval(si);
      if (c_isnan_d(tmp) || c_isinf_d(tmp)) {
	continue; 
      }
      result += tmp;
    }
  }
  //IFTRACE << "sum=" << result << endl; 
  return result;
}


std::ostream& 
Plus::dump(std::ostream& os) const
{
  os << "(";
  for (int i = 0; i < m_sz; ++i) {
    m_opands[i]->dump(os);
    if (i < m_sz-1) { 
      os << "+"; 
    }
  }
  os << ")";
  return os;
}


// ----------------------------------------------------------------------
// class Times
// ----------------------------------------------------------------------

Times::Times(EvalNode** oprnds, int numOprnds) 
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
Times::eval(const ScopeInfo* si) 
{
  double product = 1.0;
  for (int i = 0; i < m_sz; ++i) {
    double tmp = m_opands[i]->eval(si);
    if (c_isnan_d(tmp) || c_isinf_d(tmp)) {
      return c_FP_NAN_d;
    }
    product *= tmp;
  }
  //IFTRACE << "product=" << product << endl; 
  return product;
}


std::ostream& 
Times::dump(std::ostream& os) const
{
  os << "(";
  for (int i = 0; i < m_sz; ++i) {
    m_opands[i]->dump(os);
    if (i < m_sz-1) {
      os << "*";
    }
  }
  os << ")";
  return os;
}


// ----------------------------------------------------------------------
// class Min
// ----------------------------------------------------------------------

Min::Min(EvalNode** oprnds, int numOprnds) 
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
Min::eval(const ScopeInfo* si) 
{
  double result = c_FP_NAN_d;
  int i;
  for (i = 0; i < m_sz; ++i) {
    if (m_opands[i] != NULL) {
      double tmp = m_opands[i]->eval(si);
      if (c_isnan_d(tmp) || c_isinf_d(tmp)) {
	continue; 
      }
      // if i > 0, there is an empty value that we will treat as 0.0
      result = (i == 0) ? tmp : std::min(tmp, 0.0);
      break;
    }
  }
  for (++i; i < m_sz; ++i) {
    if (m_opands[i] != NULL) {
      double tmp = m_opands[i]->eval(si);
      if (c_isnan_d(tmp) || c_isinf_d(tmp)) {
	tmp = 0.0; 
      }
      result = std::min(result, tmp);
    }
  }
  //IFTRACE << "min=" << result << endl; 
  return result;
}


std::ostream& 
Min::dump(std::ostream& os) const
{
  os << "min(";
  for (int i = 0; i < m_sz; ++i) {
    m_opands[i]->dump(os);
    if (i < m_sz-1) {
      os << ",";
    }
  }
  os << ")";
  return os;
}


// ----------------------------------------------------------------------
// class Max
// ----------------------------------------------------------------------

Max::Max(EvalNode** oprnds, int numOprnds) 
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
Max::eval(const ScopeInfo* si) 
{
  double result = c_FP_NAN_d;
  int i;
  for (i = 0; i < m_sz; ++i) {
    if (m_opands[i] != NULL) {
      double tmp = m_opands[i]->eval(si);
      if (c_isnan_d(tmp) || c_isinf_d(tmp)) {
	continue; 
      }
      // if i > 0, there is an empty value that we will treat as 0.0
      result = (i == 0) ? tmp : std::max(tmp, 0.0);
      result = tmp;
      break;
    }
  }
  for (++i; i < m_sz; ++i) {
    if (m_opands[i] != NULL) {
      double tmp = m_opands[i]->eval(si);
      if (c_isnan_d(tmp) || c_isinf_d(tmp)) {
	tmp = 0.0; 
      }
      result = std::max(result, tmp);
    }
  }
  //IFTRACE << "max=" << result << endl; 
  return result;
}


std::ostream& 
Max::dump(std::ostream& os) const
{
  os << "max(";
  for (int i = 0; i < m_sz; ++i) {
    m_opands[i]->dump(os);
    if (i < m_sz-1) { 
      os << ",";
    }
  }
  os << ")";
  return os;
}
