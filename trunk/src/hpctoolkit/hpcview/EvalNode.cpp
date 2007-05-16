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
// EvalNode.C
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

#include <lib/support/Nan.h>
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
  os << std::ends;
  return os.str();
}

// ----------------------------------------------------------------------
// class Const
// ----------------------------------------------------------------------

Const::Const(double c) 
  : val(c) 
{ 
}

Const::~Const()
{ 
}

double 
Const::eval(const ScopeInfo *si) 
{
  IFTRACE << "constant=" << val << endl; 
  return val;
}

std::ostream& 
Const::dump(std::ostream& os) const
{ 
  os << "(" << val << ")"; 
  return os;
}

// ----------------------------------------------------------------------
// class Neg
// ----------------------------------------------------------------------

Neg::Neg(EvalNode* aNode) 
{ 
  node = aNode; 
}

Neg::~Neg() 
{ 
  delete node; 
}

double 
Neg::eval(const ScopeInfo *si) 
{
  double tmp;
  if (node == NULL || IsNaNorInfinity(tmp = node->eval(si)))
    return NaNVal;
  IFTRACE << "neg=" << -tmp << endl; 
  return -tmp;
}

std::ostream& 
Neg::dump(std::ostream& os) const
{ 
  os << "(-"; node->dump(os); os << ")"; 
  return os;
}

// ----------------------------------------------------------------------
// class Var
// ----------------------------------------------------------------------

Var::Var(std::string n, int i) 
  : name(n), index(i) 
{ 
}

Var::~Var() 
{ 
}

double 
Var::eval(const ScopeInfo *si) 
{
  return si->PerfData(index);
}

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
  delete base; delete exponent; 
}

double 
Power::eval(const ScopeInfo *si) 
{
  if (base == NULL || exponent == NULL)
    return NaNVal;
  double b = base->eval(si);
  double e = exponent->eval(si);
  if (IsNaNorInfinity(e) && IsNaNorInfinity(b))  // do not create a value if
                                                 // both are missing
    return (NaNVal);
  if (IsNaNorInfinity(e) || e==0)  // exp is missing, assume it is zero
    return (1);
  else
    if (IsNaNorInfinity(b) || b==0)
      return (0);
  // if b < 0, pow works only if e is integer
  // I will simplify. If b<0, return NaNVal;
  if (b < 0)
    return NaNVal;
  IFTRACE << "pow=" << pow(b, e) << endl; 
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
  delete numerator; delete denominator; 
}

double 
Divide::eval(const ScopeInfo *si) 
{
  if (numerator == NULL || denominator == NULL)
    return NaNVal;
  double n = numerator->eval(si);
  double d = denominator->eval(si);
  if (IsNaNorInfinity(d) || d == 0.0)
    return NaNVal;
  if (IsNaNorInfinity(n))
    return (0);
  IFTRACE << "divident=" << n/d << endl; 
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
}

Minus::~Minus() 
{ 
  delete minuend; delete subtrahend; 
}

double 
Minus::eval(const ScopeInfo *si) 
{
  if (minuend == NULL || subtrahend == NULL)
    return NaNVal;
  double m = minuend->eval(si);
  double s = subtrahend->eval(si);
  if (IsNaNorInfinity(m) && IsNaNorInfinity(s))
    return NaNVal;
  if (IsNaNorInfinity(m))
    m = 0.0;
  if (IsNaNorInfinity(s))
    s = 0.0;
  IFTRACE << "diff=" << m-s << endl; 
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
: nodes(oprnds), n(numOprnds) 
{ 
}

Plus::~Plus() 
{
  for (int i = 0; i < n; i++)
    delete nodes[i];
  delete[] nodes;
}

double 
Plus::eval(const ScopeInfo *si) 
{
  double result = NaNVal;
  int i;
  for (i = 0; i < n; i++) {
    if (nodes[i] != NULL) {
      double tmp = nodes[i]->eval(si);
      if (IsNaNorInfinity(tmp)) continue; 
       result = tmp;
       break;
    }
  }
  for (i++; i < n; i++) {
    if (nodes[i] != NULL) {
      double tmp = nodes[i]->eval(si);
      if (IsNaNorInfinity(tmp)) continue; 
      result += tmp;
    }
  }
  IFTRACE << "sum=" << result << endl; 
  return result;
}

std::ostream& 
Plus::dump(std::ostream& os) const
{
  os << "(";
  for (int i = 0; i < n; i++) {
      nodes[i]->dump(os);
      if (i < n-1) os << "+";
  }
  os << ")";
  return os;
}

// ----------------------------------------------------------------------
// class Times
// ----------------------------------------------------------------------

Times::Times(EvalNode** oprnds, int numOprnds) 
  : nodes(oprnds), n(numOprnds) 
{ 
}

Times::~Times() 
{
  for (int i = 0; i < n; i++)
    delete nodes[i];
  delete[] nodes;
}

double 
Times::eval(const ScopeInfo *si) 
{
  double product = 1.0;
  for (int i = 0; i < n; i++) {
    double tmp = nodes[i]->eval(si);
    if (IsNaNorInfinity(tmp))
      return NaNVal;
    product *= tmp;
  }
  IFTRACE << "product=" << product << endl; 
  return product;
}

std::ostream& 
Times::dump(std::ostream& os) const
{
  os << "(";
  for (int i = 0; i < n; i++) {
    nodes[i]->dump(os);
    if (i < n-1) os << "*";
  }
  os << ")";
  return os;
}


// ----------------------------------------------------------------------
// class Min
// ----------------------------------------------------------------------

Min::Min(EvalNode** oprnds, int numOprnds) 
  : nodes(oprnds), n(numOprnds) 
{ 
}

Min::~Min() 
{
  for (int i = 0; i < n; i++)
    delete nodes[i];
  delete[] nodes;
}

double 
Min::eval(const ScopeInfo *si) 
{
  double result = NaNVal;
  int i;
  for (i = 0; i < n; i++) {
    if (nodes[i] != NULL) {
      double tmp = nodes[i]->eval(si);
      if (IsNaNorInfinity(tmp)) continue; 
      // if i > 0, there is an empty value that we will treat as 0.0
      result = (i == 0) ? tmp : std::min(tmp, 0.0);
      break;
    }
  }
  for (i++; i < n; i++) {
    if (nodes[i] != NULL) {
      double tmp = nodes[i]->eval(si);
      if (IsNaNorInfinity(tmp)) tmp = 0.0; 
      result = std::min(result, tmp);
    }
  }
  IFTRACE << "min=" << result << endl; 
  return result;
}

std::ostream& 
Min::dump(std::ostream& os) const
{
  os << "min(";
  for (int i = 0; i < n; i++) {
    nodes[i]->dump(os);
    if (i < n-1) os << ",";
  }
  os << ")";
  return os;
}

// ----------------------------------------------------------------------
// class Max
// ----------------------------------------------------------------------

Max::Max(EvalNode** oprnds, int numOprnds) 
  : nodes(oprnds), n(numOprnds) 
{ 
}

Max::~Max() 
{
  for (int i = 0; i < n; i++)
    delete nodes[i];
  delete[] nodes;
}

double 
Max::eval(const ScopeInfo *si) 
{
  double result = NaNVal;
  int i;
  for (i = 0; i < n; i++) {
    if (nodes[i] != NULL) {
      double tmp = nodes[i]->eval(si);
      if (IsNaNorInfinity(tmp)) continue; 
      // if i > 0, there is an empty value that we will treat as 0.0
      result = (i == 0) ? tmp : std::max(tmp, 0.0);
      result = tmp;
      break;
    }
  }
  for (i++; i < n; i++) {
    if (nodes[i] != NULL) {
      double tmp = nodes[i]->eval(si);
      if (IsNaNorInfinity(tmp)) tmp = 0.0; 
      result = std::max(result, tmp);
    }
  }
  IFTRACE << "max=" << result << endl; 
  return result;
}

std::ostream& 
Max::dump(std::ostream& os) const
{
  os << "max(";

  for (int i = 0; i < n; i++) {
      nodes[i]->dump(os);
      if (i < n-1) os << ",";
  }
  os << ")";
  return os;
}
