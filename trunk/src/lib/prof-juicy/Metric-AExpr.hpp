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
// class Prof::Metric::AExpr and derived classes
//
// Currently supported expressions are
//   Const  : double constant                      : leaf
//   Var    : variable with a String name          : leaf
//   Neg    : minus times a node                   : unary
//   Power  : power expressed in base and exponent : binary
//   Divide : division expression                  : binary
//   Minus  : subtraction expression               : binary
//   Plus   : addition expression                  : n-ary
//   Times  : multiplication expression            : n-ary
//   Max    : max expression                       : n-ary
//   Min    : min expression                       : n-ary
//   Mean   : mean (arithmetic) expression         : n-ary
//   StdDev : standard deviation expression        : n-ary
//   RStdDev: relative standard deviation          : n-ary
//
// ----------------------------------------------------------------------

#ifndef prof_juicy_Prof_Metric_AExpr_hpp
#define prof_juicy_Prof_Metric_AExpr_hpp

//************************ System Include Files ******************************

#include <iostream> 
#include <string>

//************************* User Include Files *******************************

#include "PgmScopeTree.hpp"

#include <lib/support/NaN.h>

//************************ Forward Declarations ******************************

//****************************************************************************

namespace Prof {

namespace Metric {

// ----------------------------------------------------------------------
// class AExpr
//   The base class for all concrete evaluation tree node classes
// ----------------------------------------------------------------------

class AExpr
{
public:
  AExpr()
  { }

  virtual ~AExpr() 
  { }

  virtual double eval(const Struct::ANode* si) const = 0;

  static bool isok(double x) {
    return !(c_isnan_d(x) || c_isinf_d(x));
  }

  virtual std::ostream& dump(std::ostream& os = std::cout) const = 0;
  
  virtual std::string toString() const;

protected:
  static double
  eval_sum(const Struct::ANode* si, AExpr** opands, int sz) 
  {
    double result = 0.0;
    for (int i = 0; i < sz; ++i) {
      double x = opands[i]->eval(si);
      result += x;
    }
    return result;
  }

  static double
  eval_mean(const Struct::ANode* si, AExpr** opands, int sz) 
  {
    double sum = eval_sum(si, opands, sz);
    double result = sum / (double) sz;
    return result;
  }

  
  // returns <variance, mean>
  static std::pair<double, double>
  eval_variance(const Struct::ANode* si, AExpr** opands, int sz) 
  {
    double* x = new double[sz];
    
    double x_mean = 0.0; // mean
    for (int i = 0; i < sz; ++i) {
      double t = opands[i]->eval(si);
      x[i] = t;
      x_mean += t;
    }
    x_mean = x_mean / sz;
    
    double x_var = 0.0; // variance
    for (int i = 0; i < sz; ++i) {
      double t = (x[i] - x_mean);
      t = t * t;
      x_var += t;
    }
    x_var = x_var / sz;
    delete[] x;
    
    return std::make_pair(x_var, x_mean);
  }

  static void dump_opands(std::ostream& os, AExpr** opands, int sz,
			  const char* sep = ", ");
  
};


// ----------------------------------------------------------------------
// class Const
//   Represent a double constant
// ----------------------------------------------------------------------

class Const : public AExpr
{
public:
  Const(double c) 
    : m_c(c) 
  { }

  ~Const() 
  { }

  double eval(const Struct::ANode* si) const
  {
    return m_c;
  }

  std::ostream& dump(std::ostream& os = std::cout) const;
  
private:
  double m_c;
};


// ----------------------------------------------------------------------
// class Neg
//   Represent a negative value of an AExpr
// ----------------------------------------------------------------------

class Neg : public AExpr
{  
public:
  // Assumes ownership of AExpr
  Neg(AExpr* expr)
  { 
    m_expr = expr; 
  }

  ~Neg()
  { 
    delete m_expr; 
  }

  double eval(const Struct::ANode* si) const;

  std::ostream& dump(std::ostream& os = std::cout) const;
  
private:
  AExpr* m_expr;
};


// ----------------------------------------------------------------------
// class Var
//   Represent a variable -- the evaluator better deals with the symbol
//   table
// ----------------------------------------------------------------------

class Var : public AExpr
{
public:
  Var(std::string n, int i)
    : name(n), index(i) 
  { 
  }
  
  ~Var()
  { 
  }

  double eval(const Struct::ANode* si) const
  {
    return si->PerfData(index);
  }
  
  std::ostream& dump(std::ostream& os = std::cout) const;
  
private:
  std::string name;
  int index;
};


// ----------------------------------------------------------------------
// class Power
//   Represent a power expression
// ----------------------------------------------------------------------

class Power : public AExpr
{
public:
  // Assumes ownership of AExpr
  Power(AExpr* b, AExpr* e);
  ~Power();
  double eval(const Struct::ANode* si) const;
  std::ostream& dump(std::ostream& os = std::cout) const;

private:
  AExpr* base;
  AExpr* exponent;
};


// ----------------------------------------------------------------------
// class Divide
//   Represent the division
// ----------------------------------------------------------------------

class Divide : public AExpr
{
public:
  // Assumes ownership of AExpr
  Divide(AExpr* num, AExpr* denom);
  ~Divide();

  double eval(const Struct::ANode* si) const;

  std::ostream& dump(std::ostream& os = std::cout) const;

private:
  AExpr* numerator;
  AExpr* denominator;
};


// ----------------------------------------------------------------------
// class Minus
//   Represent the subtraction
// ----------------------------------------------------------------------

class Minus : public AExpr
{
public:
  // Assumes ownership of AExpr
  Minus(AExpr* m, AExpr* s);
  ~Minus();

  double eval(const Struct::ANode* si) const;

  std::ostream& dump(std::ostream& os = std::cout) const;

private:
  AExpr* minuend;
  AExpr* subtrahend;
};


// ----------------------------------------------------------------------
// class Plus
//   Represent addition
// ----------------------------------------------------------------------

class Plus : public AExpr
{
public:
  // Assumes ownership of AExpr
  Plus(AExpr** oprnds, int numOprnds);
  ~Plus();
  
  double eval(const Struct::ANode* si) const;

  std::ostream& dump(std::ostream& os = std::cout) const;

private:
  AExpr** m_opands;
  int m_sz;
};


// ----------------------------------------------------------------------
// class Times
//   Represent multiplication
// ----------------------------------------------------------------------

class Times : public AExpr
{
public:
  // Assumes ownership of AExpr
  Times(AExpr** oprnds, int numOprnds);
  ~Times();

  double eval(const Struct::ANode* si) const;

  std::ostream& dump(std::ostream& os = std::cout) const;

private:
  AExpr** m_opands;
  int m_sz;
};


// ----------------------------------------------------------------------
// Max
// ----------------------------------------------------------------------

class Max : public AExpr
{
public:
  // Assumes ownership of AExpr
  Max(AExpr** oprnds, int numOprnds);
  ~Max();

  double eval(const Struct::ANode* si) const;

  std::ostream& dump(std::ostream& os = std::cout) const;

private:
  AExpr** m_opands;
  int m_sz;
};


// ----------------------------------------------------------------------
// Min
// ----------------------------------------------------------------------

class Min : public AExpr
{
public:
  // Assumes ownership of AExpr
  Min(AExpr** oprnds, int numOprnds);
  ~Min();

  double eval(const Struct::ANode* si) const;

  std::ostream& dump(std::ostream& os = std::cout) const;

private:
  AExpr** m_opands;
  int m_sz;
};


// ----------------------------------------------------------------------
// Mean
// ----------------------------------------------------------------------

class Mean : public AExpr
{
public:
  // Assumes ownership of AExpr
  Mean(AExpr** oprnds, int numOprnds);
  ~Mean();

  double eval(const Struct::ANode* si) const;

  std::ostream& dump(std::ostream& os = std::cout) const;

private:
  AExpr** m_opands;
  int m_sz;
};


// ----------------------------------------------------------------------
// StdDev: standard deviation
// ----------------------------------------------------------------------

class StdDev : public AExpr
{
public:
  // Assumes ownership of AExpr
  StdDev(AExpr** oprnds, int numOprnds);
  ~StdDev();

  double eval(const Struct::ANode* si) const;

  std::ostream& dump(std::ostream& os = std::cout) const;

private:
  AExpr** m_opands;
  int m_sz;
};


// ----------------------------------------------------------------------
// RStdDev: relative standard deviation
// ----------------------------------------------------------------------

class RStdDev : public AExpr
{
public:
  // Assumes ownership of AExpr
  RStdDev(AExpr** oprnds, int numOprnds);
  ~RStdDev();

  double eval(const Struct::ANode* si) const;

  std::ostream& dump(std::ostream& os = std::cout) const;

private:
  AExpr** m_opands;
  int m_sz;
};


//****************************************************************************

} // namespace Metric

} // namespace Prof

//****************************************************************************

#endif /* prof_juicy_Prof_Metric_AExpr_hpp */
