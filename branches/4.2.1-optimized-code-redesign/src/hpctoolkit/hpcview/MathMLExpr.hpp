// $Id$
// -*-C++-*-
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
// MathMLExpr.h
//   header file for the implementation of arithmetic evaluation of
//   metrics
//
// ----------------------------------------------------------------------

#ifndef MathMLExpr_H
#define MathMLExpr_H

//************************ System Include Files ******************************

//************************ Xerces Include Files ******************************

// class DOMNode;
#include <xercesc/dom/DOMNode.hpp>         /* johnmc */
using XERCES_CPP_NAMESPACE::DOMNode;

//************************* User Include Files *******************************

// #include "XMLAdapter.h"
#include "EvalNode.hpp"

#include <lib/prof-juicy/PgmScopeTree.hpp>

#include <lib/support/String.hpp>


//************************ Forward Declarations ******************************

//****************************************************************************

// ----------------------------------------------------------------------
// class MathMLException
//   Exceptions thrown during the construction of MathMLExpr object or
//   evaluation of MathMLExpr
// ----------------------------------------------------------------------

class MathMLExprException
{

public:
  MathMLExprException(String aName) {
    name = aName;
  }
  MathMLExprException() : name("") { }
  ~MathMLExprException() { }
  String getMessage() const { return name; };
  
private:
  String name;
};


// ----------------------------------------------------------------------
//
// class MathMLExpr
//   The expressive power of this MathMLExpr class is determined by the
//   kinds of nodes supported by class EvalNode.  Only the content
//   markup part of the MathML is employed here.  Currently, only the
//   following is supported: (the corresponding MathML content mark up
//   is include in the parenthesis)
//     unary  -- constant (<cn></cn>), variable (<ci></ci>) and
//               minus (<minus/>)
//     binary -- power (<power/>), divide (<divide/>) and minus (<minus/>)
//     Nary   -- plus (<plus/>) and times (<times/>)
//
//   A sample of MathML expression is as follows:
//      <apply>
//         <plus/>
//         <apply>
//            <power/>
//            <ci>x</ci>
//            <cn>2</cn>
//         </apply>
//         <apply>
//            <times/>
//            <cn>4</cn>
//            <ci>x</ci>
//         </apply>
//         <cn>4</cn>
//      </apply>
//   It is equal to:  x ^ 2 + 4 * x + 4.
//
// ----------------------------------------------------------------------

class MathMLExpr
{

public:

  // ------------------------------------------------------------
  // -- MathMLExpr(const XMLCh* inputSource) --
  //   Build a new MathMLExpr object out of the specified input string.
  //   This string contains MathML expressions.
  // -- params --
  //   inputSource:    input string in XMLCh format
  // -- exception --
  //   MathMLExprException could be thrown due to invalid or unsupported
  //   MathML expressions.
  // ------------------------------------------------------------
  
  MathMLExpr(DOMNode *math);

  // ------------------------------------------------------------
  // -- ~MathMLExpr() --
  //   Destructor.
  // ------------------------------------------------------------
  
  ~MathMLExpr();

  // ------------------------------------------------------------
  // -- double eval() --
  //   Evaluate the MathML expression.  
  // ------------------------------------------------------------

  double eval(const ScopeInfo *si);

  // ------------------------------------------------------------
  // -- void print() --
  //   Simple printout of nodes.
  // ------------------------------------------------------------
  
  void print();
  
private:
  EvalNode* topNode;
  bool isNumber;           // not sure about this
  // helper function
  EvalNode* buildEvalTree(DOMNode *node);
};

#endif  // MathMLExpr_H
