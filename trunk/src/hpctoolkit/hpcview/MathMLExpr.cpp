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

//************************ System Include Files ******************************

#include <iostream> 

#ifdef NO_STD_CHEADERS
# include <stdlib.h>
#else
# include <cstdlib>
using std::atof; // For compatibility with non-std C headers
using std::atoi;
#endif

//************************* User Include Files *******************************

#include "XMLAdapter.h"
#include "MathMLExpr.h"
#include "PerfMetric.h"
#include <lib/support/String.h>
#include <lib/support/Nan.h>
#include <lib/support/Trace.h>

//************************ Forward Declarations ******************************

using std::cerr;
using std::endl;

//****************************************************************************

// ----------------------------------------------------------------------
//
// class MathMLExpr
//   The expressive power of this MathMLExpr class is determined by the
//   kinds of nodes supported by class EvalNode.  Currently, only the
//   following is supported: (the corresponding MathMLML content mark up
//   is include in the parenthesis)
//     unary  -- constant (<cn></cn>), variable (<ci></ci>) and
//               minus (<minus/>)
//     binary -- power (<power/>), divide (<divide/>) and minus (<minus/>)
//     Nary   -- plus (<plus/>) and times (<times/>)
//
//   A sample of MathMLML expression is as follows:
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

// ----------------------------------------------------------------------
//
// Declarations of helper functions
//
// ----------------------------------------------------------------------

static bool isOperatorNode(DOM_Node& node);
static bool isOperandNode(DOM_Node& node);

extern double NaNVal;

MathMLExpr::MathMLExpr(DOM_Node &math) 
{
  int exprs = 0;
  DOM_Node child = math.getFirstChild();
  for (; child != NULL; child = child.getNextSibling()) {
    if (child.getNodeType() == DOM_Node::TEXT_NODE) {
      // DTD ensures this can't contain anything but white space
      continue;
    } else if (child.getNodeType() == DOM_Node::COMMENT_NODE) {
      continue;
    }
    
    if (exprs == 1) {
      throw MathMLExprException("More than one MathML expression specified for COMPUTE");
    }
    IFTRACE << child.getNodeType() << endl; 
    exprs++;
    topNode = buildEvalTree(child);
  }
  if (exprs == 0) {
    throw MathMLExprException("No MathML expression specified for COMPUTE");
  }
}

MathMLExpr::~MathMLExpr() 
{
  delete topNode;
}

double MathMLExpr::eval(const ScopeInfo *si) 
{
  IFDOTRACE { si->DumpSelf(cerr); }
  if (topNode != NULL)
    return topNode->eval(si);
  return NaNVal;
}

// ----------------------------------------------------------------------
// -- bool isOperatorNode(DOM_Node& node) --
//   Tests whether a DOM_Node is one of the operator nodes -- divide,
//   minus, plus, power and times.
// -- params --
//   node:       a DOM_Node
// ----------------------------------------------------------------------

static bool isOperatorNode(DOM_Node& node) {
  
  static const DOMString DIVISION("divide");
  static const DOMString SUBTRACTION("minus");
  static const DOMString ADDITION("plus");
  static const DOMString MAXML("max");
  static const DOMString MINML("min");
  static const DOMString EXPONENTIATION("power");
  static const DOMString MULTIPLICATION("times");

  DOMString nodeName = node.getNodeName();

  return (nodeName.equals(DIVISION) || nodeName.equals(SUBTRACTION) ||
	  nodeName.equals(ADDITION) || nodeName.equals(EXPONENTIATION) ||
	  nodeName.equals(MULTIPLICATION) || nodeName.equals(MAXML) || 
		nodeName.equals(MINML));
}

static bool isOperandNode(DOM_Node& node) {

  static const DOMString APPLY("apply");
  static const DOMString NUMBER("cn");
  static const DOMString IDENTIFIER("ci");

  DOMString nodeName = node.getNodeName();

  return (nodeName.equals(APPLY) ||
	  nodeName.equals(NUMBER) || nodeName.equals(IDENTIFIER));
}


// ----------------------------------------------------------------------
// -- EvalNode* buildEvalTree(DOM_Node& node) --
//   Build an EvalNode object out of a DOM_Node object.
// -- params --
//   node:    a DOM_Node
// -- exception --
//   An exception of type MathMLExprException could be thrown.  If the 
// ----------------------------------------------------------------------

EvalNode* MathMLExpr::buildEvalTree(DOM_Node& node) {

  static const DOMString MathML("MathML");
  static const DOMString APPLY("apply");
  static const DOMString DIVISION("divide");
  static const DOMString SUBTRACTION("minus");
  static const DOMString ADDITION("plus");
  static const DOMString EXPONENTIATION("power");
  static const DOMString MULTIPLICATION("times");
  static const DOMString MAXML("max");
  static const DOMString MINML("min");
  static const DOMString NUMBER("cn");
  static const DOMString IDENTIFIER("ci");

  // Get the name and value out for convenience
  DOMString nodeName = node.getNodeName();
  DOMString nodeValue = node.getNodeValue();

  switch (node.getNodeType()) {
    
  case DOM_Node::TEXT_NODE:
    {
      EvalNode* evalNode;
      const XMLCh* xmlStr = nodeValue.rawBuffer();
      if (isNumber) {  // is a number
	char* str = XMLStrToStr(xmlStr);
	IFTRACE << "str --" << str << "--" << endl; 
	evalNode = new Const(atof(str));
	IFTRACE << "number is --" << atof(str) << "--" << endl; 
	delete[] str;
      }
      else {           // is a variable
	String str = XMLStrToString(xmlStr);
	IFTRACE << "str --" << str << "--" << endl; 
	int perfDataIndex = NameToPerfDataIndex(str);
	IFTRACE << "index --" << perfDataIndex << "--" << endl; 
	if (!IsPerfDataIndex(perfDataIndex)) {
	  String error = String("Undefined metric '") + str + "' encountered";
	  throw MathMLExprException(error);
	}
	evalNode = new Var(str, perfDataIndex);
      }
      return evalNode;
    }
  
  case DOM_Node::ELEMENT_NODE: { // ignore all attributes

    DOM_Node child = node.getFirstChild();

    // if it is an apply node
    if (nodeName.equals(APPLY)) {
      IFTRACE << nodeName << endl; 

      // find the node that could be the operator
      // if it is not, throw a "no operator" exception
      DOM_Node op = child;
      while (op != NULL && op.getNodeType() != DOM_Node::ELEMENT_NODE)
	op = op.getNextSibling();
      if (op == NULL || !isOperatorNode(op)) {
	throw MathMLExprException("No operator");
	return NULL;
      }

      // if the operator is found
      int numChildren = 0;
      child = op;
      while ((child = child.getNextSibling()) != NULL) {
	// could be some text between operands, ignore them
	if (child.getNodeType() == DOM_Node::TEXT_NODE) {
	  continue;
	} else if (child.getNodeType() == DOM_Node::COMMENT_NODE) {
	  continue;
	}
	
	if (isOperandNode(child))
	  numChildren++;
	else {
	  throw MathMLExprException("Invalid expression");
	  return NULL;
	}
      }
      IFTRACE << numChildren << " children" << endl; 
      
      if (numChildren == 0) {
	throw MathMLExprException("No operand");
	return NULL;
      }

      EvalNode** nodes = new EvalNode*[numChildren];

      child = op;
      for (int i = 0; i < numChildren; i++) {
	do {
	  child = child.getNextSibling();
	} while (!isOperandNode(child));
	IFTRACE << "child " << i << " " << child << endl; 
	try {
	  nodes[i] = buildEvalTree(child);
	}
	catch (const MathMLExprException& e) {
	  for (int j = 0; j < i; j++)
	    delete nodes[j];
	  delete[] nodes;
	  throw;
	  return NULL;
	}
      }

      nodeName = op.getNodeName();
      if (nodeName.equals(ADDITION))
	return new Plus(nodes, numChildren);
      if (nodeName.equals(SUBTRACTION) && numChildren <= 2) {
	if (numChildren == 1)
	  return new Neg(nodes[0]);
	return new Minus(nodes[0], nodes[1]);
      }
      if (nodeName.equals(EXPONENTIATION) && numChildren == 2)
	return new Power(nodes[0], nodes[1]);
      if (nodeName.equals(MULTIPLICATION))
	return new Times(nodes, numChildren);
      if (nodeName.equals(MAXML))
	return new Max(nodes, numChildren);
      if (nodeName.equals(MINML))
	return new Min(nodes, numChildren);
      if (nodeName.equals(DIVISION) && numChildren == 2)
	return new Divide(nodes[0], nodes[1]);
      // otherwise, throw an exception and return NULL after the switch
      // switch

      for (int i = 0; i < numChildren; i++)
	delete nodes[i];
      delete[] nodes;
      throw MathMLExprException("Unknown operation");
    }

    if (nodeName.equals(NUMBER)) {
      IFTRACE << "--" << child << "--" << endl; 
      isNumber = true;
      EvalNode* builtNode;
      try {
	builtNode = buildEvalTree(child);
	return builtNode;
      }
      catch (const MathMLExprException& e) {
	throw;
      }
    }

    if (nodeName.equals(IDENTIFIER)) {
      IFTRACE << "--" << child << "--" << endl; 
      isNumber = false;
      EvalNode* builtNode;
      try {
	builtNode = buildEvalTree(child);
	return builtNode;
      }
      catch (const MathMLExprException& e) {
	throw;
      }
    }

    IFTRACE << nodeName << endl; 
    throw MathMLExprException("Unknown node");
  }

  case DOM_Node::ENTITY_REFERENCE_NODE:
    {
      throw MathMLExprException("Entity -- not supported");
    }
  
  case DOM_Node::CDATA_SECTION_NODE:
    {
      throw MathMLExprException("CDATA -- not supported");
    }
  
  case DOM_Node::COMMENT_NODE:
    {
      //throw MathMLExprException("Comment -- not supported");
      break;
    }
  
  default:
    throw MathMLExprException("Unknown -- not supported");
  }

  return NULL; // should not reach
}

void MathMLExpr::print() { topNode->print(); }
