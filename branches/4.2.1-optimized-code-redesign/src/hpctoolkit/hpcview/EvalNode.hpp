// -*-Mode: C++;-*-
// $Id$

// * BeginRiceCopyright *****************************************************
// 
// Copyright ((c)) 2002, Rice University 
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
// class EvalNode and derived node classes
//   Define the evaluation tree nodes.  Currently supported nodes are
//     Const --  a double constant
//     Var   --  a variable with a String name
//     Neg   --  a minus times a node
//     Power --  a power expressed in base and exponent
//     Divide--  a division expression
//     Minus --  a subtraction expression
//     Plus  --  an addition expression
//     Times --  a multiplication expression
//     Max   --  max expression
//     Min   --  min expression
//   Const and Var are leaf nodes.  Neg is a unary node.  Divide, Power
//   and Minus are binary nodes.  Plus and Times are n-ary nodes.
//
//   During the evaluation of EvalNode objects, a NAN will be returned
//   in the following cases:
//   1)  a constant or variable corresponds to a NAN
//   2)  any part of unary, binary or N-ary operations evaluates to
//       a NAN
//   3)  divide by zero
//
// ----------------------------------------------------------------------

#ifndef EVALNODE_H
#define EVALNODE_H

//************************ System Include Files ******************************

#include <iostream> 
#include <string>

//************************* User Include Files *******************************

#include <lib/prof-juicy/PgmScopeTree.hpp>

#include <lib/support/String.hpp>

//************************ Forward Declarations ******************************

//****************************************************************************

// ----------------------------------------------------------------------
// class EvalNode
//   The base class for all concrete evaluation tree node classes
// ----------------------------------------------------------------------

class EvalNode
{
public:
  EvalNode() { }
  virtual ~EvalNode() { }
  virtual double eval(const ScopeInfo *si) = 0;
  virtual std::ostream& dump(std::ostream& os = std::cout) const = 0;
  
  virtual std::string toString() const;
};

// ----------------------------------------------------------------------
// class Const
//   Represent a double constant
// ----------------------------------------------------------------------

class Const : public EvalNode
{
public:
  Const(double c);
  ~Const();
  double eval(const ScopeInfo *si);
  std::ostream& dump(std::ostream& os = std::cout) const;
  
private:
  double val;
};

// ----------------------------------------------------------------------
// class Neg
//   Represent a negative value of an EvalNode
// ----------------------------------------------------------------------

class Neg : public EvalNode
{  
public:
  Neg(EvalNode* node);
  ~Neg();
  double eval(const ScopeInfo *si);
  std::ostream& dump(std::ostream& os = std::cout) const;
  
private:
  EvalNode* node;
};

// ----------------------------------------------------------------------
// class Var
//   Represent a variable -- the evaluator better deals with the symbol
//   table
// ----------------------------------------------------------------------

class Var : public EvalNode
{
public:
  Var(String n, int i);
  ~Var();
  double eval(const ScopeInfo *si);
  std::ostream& dump(std::ostream& os = std::cout) const;
  
private:
  String name;
  int index;
};

// ----------------------------------------------------------------------
// class Power
//   Represent a power expression
// ----------------------------------------------------------------------

class Power : public EvalNode
{
public:
  Power(EvalNode* b, EvalNode* e);
  ~Power();
  double eval(const ScopeInfo *si);
  std::ostream& dump(std::ostream& os = std::cout) const;

private:
  EvalNode* base;
  EvalNode* exponent;
};

// ----------------------------------------------------------------------
// class Divide
//   Represent the division
// ----------------------------------------------------------------------

class Divide : public EvalNode
{
public:
  Divide(EvalNode* num, EvalNode* denom);
  ~Divide();
  double eval(const ScopeInfo *si);
  std::ostream& dump(std::ostream& os = std::cout) const;

private:
  EvalNode* numerator;
  EvalNode* denominator;
};

// ----------------------------------------------------------------------
// class Minus
//   Represent the subtraction
// ----------------------------------------------------------------------

class Minus : public EvalNode
{
public:
  Minus(EvalNode* m, EvalNode* s);
  ~Minus();
  double eval(const ScopeInfo *si);
  std::ostream& dump(std::ostream& os = std::cout) const;

private:
  EvalNode* minuend;
  EvalNode* subtrahend;
};

// ----------------------------------------------------------------------
// class Plus
//   Represent addition
// ----------------------------------------------------------------------

class Plus : public EvalNode
{
public:
  Plus(EvalNode** oprnds, int numOprnds);
  ~Plus();
  double eval(const ScopeInfo *si);
  std::ostream& dump(std::ostream& os = std::cout) const;

private:
  EvalNode** nodes;
  int n;
};

// ----------------------------------------------------------------------
// class Times
//   Represent multiplication
// ----------------------------------------------------------------------

class Times : public EvalNode
{
public:
  Times(EvalNode** oprnds, int numOprnds);
  ~Times();
  double eval(const ScopeInfo *si);
  std::ostream& dump(std::ostream& os = std::cout) const;

private:
  EvalNode** nodes;
  int n;
};


class Max : public EvalNode
{
public:
  Max(EvalNode** oprnds, int numOprnds);
  ~Max();
  double eval(const ScopeInfo *si);
  std::ostream& dump(std::ostream& os = std::cout) const;

private:
  EvalNode** nodes;
  int n;
};

class Min : public EvalNode
{
public:
  Min(EvalNode** oprnds, int numOprnds);
  ~Min();
  double eval(const ScopeInfo *si);
  std::ostream& dump(std::ostream& os = std::cout) const;

private:
  EvalNode** nodes;
  int n;
};

#endif  // EVALNODE_H
