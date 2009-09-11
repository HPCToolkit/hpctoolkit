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
// class Prof::Metric::AExpr and derived classes
//
// Currently supported expressions are
//   Const  : double constant                      : leaf
//   Var    : variable with a String name          : leaf
//   Neg    : negation                             : unary
//   Power  : power expressed in base and exponent : binary
//   Divide : division expression                  : binary
//   Minus  : subtraction expression               : binary
//   Plus   : addition expression                  : n-ary
//   Times  : multiplication expression            : n-ary
//   Max    : max expression                       : n-ary
//   Min    : min expression                       : n-ary
//   Mean   : mean (arithmetic) expression         : n-ary
//   StdDev : standard deviation expression        : n-ary
//   CoefVar: coefficient of variance              : n-ary
//   RStdDev: relative standard deviation          : n-ary
//
// ----------------------------------------------------------------------

#ifndef prof_juicy_Prof_Metric_AExpr_hpp
#define prof_juicy_Prof_Metric_AExpr_hpp

//************************ System Include Files ******************************

#include <iostream> 
#include <string>

//************************* User Include Files *******************************

#include "Metric-IData.hpp"

#include <lib/support/NaN.h>

//************************ Forward Declarations ******************************

//****************************************************************************

namespace Prof {

namespace Metric {

// ----------------------------------------------------------------------
// class AExpr
//   The base class for all concrete evaluation classes
// ----------------------------------------------------------------------

class AExpr
{
  // TODO: replace AExpr** with AExprVec
public:
  typedef std::vector<AExpr*> AExprVec;

public:
  AExpr()
  { }

  virtual ~AExpr() 
  { }

  virtual double 
  eval(const Metric::IData& mdata) const = 0;

  static bool 
  isok(double x) 
  { return !(c_isnan_d(x) || c_isinf_d(x)); }

  virtual std::ostream& 
  dump(std::ostream& os = std::cout) const = 0;
  
  virtual std::string 
  toString() const;

protected:
  static double
  evalSum(const Metric::IData& mdata, AExpr** opands, int sz) 
  {
    double result = 0.0;
    for (int i = 0; i < sz; ++i) {
      double x = opands[i]->eval(mdata);
      result += x;
    }
    return result;
  }

  static double
  evalMean(const Metric::IData& mdata, AExpr** opands, int sz) 
  {
    double sum = evalSum(mdata, opands, sz);
    double result = sum / (double) sz;
    return result;
  }

  
  // returns <variance, mean>
  static std::pair<double, double>
  evalVariance(const Metric::IData& mdata, AExpr** opands, int sz) 
  {
    double* x = new double[sz];
    
    double x_mean = 0.0; // mean
    for (int i = 0; i < sz; ++i) {
      double t = opands[i]->eval(mdata);
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

  static void 
  dump_opands(std::ostream& os, AExpr** opands, int sz, const char* sep = ", ");
  
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

  double 
  eval(const Metric::IData& mdata) const
  { return m_c; }

  std::ostream& 
  dump(std::ostream& os = std::cout) const;
  
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
  { m_expr = expr; }

  ~Neg()
  { delete m_expr; }

  double 
  eval(const Metric::IData& mdata) const;

  std::ostream& 
  dump(std::ostream& os = std::cout) const;
  
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
  Var(std::string name, int metricId)
    : m_name(name), m_metricId(metricId) 
  { }
  
  ~Var()
  { }

  double 
  eval(const Metric::IData& mdata) const
  { return mdata.demandMetric(m_metricId); }
  
  std::ostream& 
  dump(std::ostream& os = std::cout) const;
  
private:
  std::string m_name;
  int m_metricId;
};


// ----------------------------------------------------------------------
// class Power
//   Represent a power expression
// ----------------------------------------------------------------------

class Power : public AExpr
{
public:
  // Assumes ownership of AExpr
  Power(AExpr* base, AExpr* exponent)
    : m_base(base), m_exponent(exponent) 
  { }

  ~Power()
  { 
    delete m_base; 
    delete m_exponent; 
  }

  double
  eval(const Metric::IData& mdata) const;

  std::ostream&
  dump(std::ostream& os = std::cout) const;

private:
  AExpr* m_base;
  AExpr* m_exponent;
};


// ----------------------------------------------------------------------
// class Divide
//   Represent the division
// ----------------------------------------------------------------------

class Divide : public AExpr
{
public:
  // Assumes ownership of AExpr
  Divide(AExpr* numerator, AExpr* denominator)
    : m_numerator(numerator), m_denominator(denominator) 
  { }

  ~Divide()
  { 
    delete m_numerator; 
    delete m_denominator; 
  }


  double
  eval(const Metric::IData& mdata) const;

  std::ostream&
  dump(std::ostream& os = std::cout) const;

private:
  AExpr* m_numerator;
  AExpr* m_denominator;
};


// ----------------------------------------------------------------------
// class Minus
//   Represent the subtraction
// ----------------------------------------------------------------------

class Minus : public AExpr
{
public:
  // Assumes ownership of AExpr
  Minus(AExpr* minuend, AExpr* subtrahend)
    : m_minuend(minuend), m_subtrahend(subtrahend) 
  { }

  ~Minus()
  {
    delete m_minuend;
    delete m_subtrahend;
  }

  double 
  eval(const Metric::IData& mdata) const;

  std::ostream& 
  dump(std::ostream& os = std::cout) const;

private:
  AExpr* m_minuend;
  AExpr* m_subtrahend;
};


// ----------------------------------------------------------------------
// class Plus
//   Represent addition
// ----------------------------------------------------------------------

class Plus : public AExpr
{
public:
  // Assumes ownership of AExpr
  Plus(AExpr** oprnds, int numOprnds)
    : m_opands(oprnds), m_sz(numOprnds) 
  { }

  ~Plus();
  
  double 
  eval(const Metric::IData& mdata) const;

  std::ostream& 
  dump(std::ostream& os = std::cout) const;

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
  Times(AExpr** oprnds, int numOprnds)
    : m_opands(oprnds), m_sz(numOprnds) 
  { }

  ~Times();

  double 
  eval(const Metric::IData& mdata) const;

  std::ostream& 
  dump(std::ostream& os = std::cout) const;

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
  Max(AExpr** oprnds, int numOprnds)
    : m_opands(oprnds), m_sz(numOprnds) 
  { }

  ~Max();

  double 
  eval(const Metric::IData& mdata) const;

  std::ostream& 
  dump(std::ostream& os = std::cout) const;

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
  Min(AExpr** oprnds, int numOprnds)
    : m_opands(oprnds), m_sz(numOprnds) 
  { }

  ~Min();

  double
  eval(const Metric::IData& mdata) const;

  std::ostream&
  dump(std::ostream& os = std::cout) const;

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
  Mean(AExpr** oprnds, int numOprnds)
    : m_opands(oprnds), m_sz(numOprnds) 
  { }

  ~Mean();

  double
  eval(const Metric::IData& mdata) const;

  std::ostream&
  dump(std::ostream& os = std::cout) const;

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
  StdDev(AExpr** oprnds, int numOprnds)
    : m_opands(oprnds), m_sz(numOprnds) 
  { }

  ~StdDev();

  double
  eval(const Metric::IData& mdata) const;

  std::ostream&
  dump(std::ostream& os = std::cout) const;

private:
  AExpr** m_opands;
  int m_sz;
};


// ----------------------------------------------------------------------
// CoefVar: relative standard deviation
// ----------------------------------------------------------------------

class CoefVar : public AExpr
{
public:
  // Assumes ownership of AExpr
  CoefVar(AExpr** oprnds, int numOprnds)
    : m_opands(oprnds), m_sz(numOprnds) 
  { }

  ~CoefVar();

  double
  eval(const Metric::IData& mdata) const;

  std::ostream&
  dump(std::ostream& os = std::cout) const;

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
  RStdDev(AExpr** oprnds, int numOprnds)
    : m_opands(oprnds), m_sz(numOprnds) 
  { }

  ~RStdDev();

  double
  eval(const Metric::IData& mdata) const;

  std::ostream&
  dump(std::ostream& os = std::cout) const;

private:
  AExpr** m_opands;
  int m_sz;
};


//****************************************************************************

} // namespace Metric

} // namespace Prof

//****************************************************************************

#endif /* prof_juicy_Prof_Metric_AExpr_hpp */
