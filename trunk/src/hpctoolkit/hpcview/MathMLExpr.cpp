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

// #include "XMLAdapter.h"
#include "MathMLExpr.hpp"
#include "PerfMetric.hpp"
#include <lib/support/String.hpp>
#include <lib/support/Nan.h>
#include <lib/support/Trace.hpp>

//************************ Xerces Include Files ******************************

#include <xercesc/util/XMLString.hpp>         
using XERCES_CPP_NAMESPACE::XMLString;


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

static bool isOperatorNode(DOMNode *node);
static bool isOperandNode(DOMNode *node);

extern double NaNVal;

MathMLExpr::MathMLExpr(DOMNode *math) 
{
  int exprs = 0;
  DOMNode *child = math->getFirstChild();
  for (; child != NULL; child = child->getNextSibling()) {
    if (child->getNodeType() == DOMNode::TEXT_NODE) {
      // DTD ensures this can't contain anything but white space
      continue;
    } else if (child->getNodeType() == DOMNode::COMMENT_NODE) {
      continue;
    }
    
    if (exprs == 1) {
      throw MathMLExprException("More than one MathML expression specified for COMPUTE");
    }
    IFTRACE << child->getNodeType() << endl; 
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
// -- bool isOperatorNode(DOMNode *node) --
//   Tests whether a DOMNode is one of the operator nodes -- divide,
//   minus, plus, power and times.
// -- params --
//   node:       a DOMNode
// ----------------------------------------------------------------------

static bool isOperatorNode(DOMNode *node) {
  
  static const XMLCh* DIVISION = XMLString::transcode("divide");
  static const XMLCh* SUBTRACTION = XMLString::transcode("minus");
  static const XMLCh* ADDITION = XMLString::transcode("plus");
  static const XMLCh* MAXML = XMLString::transcode("max");
  static const XMLCh* MINML = XMLString::transcode("min");
  static const XMLCh* EXPONENTIATION = XMLString::transcode("power");
  static const XMLCh* MULTIPLICATION = XMLString::transcode("times");

  const XMLCh* nodeName = node->getNodeName();

  return (XMLString::equals(nodeName,DIVISION) || XMLString::equals(nodeName,SUBTRACTION) ||
	  XMLString::equals(nodeName,ADDITION) || XMLString::equals(nodeName,EXPONENTIATION) ||
	  XMLString::equals(nodeName,MULTIPLICATION) || XMLString::equals(nodeName,MAXML) || 
		XMLString::equals(nodeName,MINML));
}

static bool isOperandNode(DOMNode *node) {

  static const XMLCh* APPLY = XMLString::transcode("apply");
  static const XMLCh* NUMBER = XMLString::transcode("cn");
  static const XMLCh* IDENTIFIER = XMLString::transcode("ci");

  const XMLCh* nodeName = node->getNodeName();

  return (XMLString::equals(nodeName,APPLY) ||
	  XMLString::equals(nodeName,NUMBER) || XMLString::equals(nodeName,IDENTIFIER));
}


// ----------------------------------------------------------------------
// -- EvalNode* buildEvalTree(DOMNode *node) --
//   Build an EvalNode object out of a DOMNode object.
// -- params --
//   node:    a DOMNode
// -- exception --
//   An exception of type MathMLExprException could be thrown.  If the 
// ----------------------------------------------------------------------

EvalNode* MathMLExpr::buildEvalTree(DOMNode *node) {

  static const XMLCh* APPLY = XMLString::transcode("apply");
  static const XMLCh* DIVISION = XMLString::transcode("divide");
  static const XMLCh* SUBTRACTION = XMLString::transcode("minus");
  static const XMLCh* ADDITION = XMLString::transcode("plus");
  static const XMLCh* EXPONENTIATION = XMLString::transcode("power");
  static const XMLCh* MULTIPLICATION = XMLString::transcode("times");
  static const XMLCh* MAXML = XMLString::transcode("max");
  static const XMLCh* MINML = XMLString::transcode("min");
  static const XMLCh* NUMBER = XMLString::transcode("cn");
  static const XMLCh* IDENTIFIER = XMLString::transcode("ci");

  // Get the name and value out for convenience
  const XMLCh* nodeName = node->getNodeName();
  const XMLCh* nodeValue = node->getNodeValue();

  switch (node->getNodeType()) {
    
  case DOMNode::TEXT_NODE:
    {
      EvalNode* evalNode;
      if (isNumber) {  // is a number
	char* str = XMLString::transcode(nodeValue);
	IFTRACE << "str --" << str << "--" << endl; 
	evalNode = new Const(atof(str));
	IFTRACE << "number is --" << atof(str) << "--" << endl; 
	delete[] str;
      }
      else {           // is a variable
	String str = XMLString::transcode(nodeValue);
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
  
  case DOMNode::ELEMENT_NODE: { // ignore all attributes

    DOMNode *child = node->getFirstChild();

    // if it is an apply node
    if (XMLString::equals(nodeName,APPLY)) {
      IFTRACE << nodeName << endl; 

      // find the node that could be the operator
      // if it is not, throw a "no operator" exception
      DOMNode *op = child;
      while (op != NULL && op->getNodeType() != DOMNode::ELEMENT_NODE)
	op = op->getNextSibling();
      if (op == NULL || !isOperatorNode(op)) {
	throw MathMLExprException("No operator");
	return NULL;
      }

      // if the operator is found
      int numChildren = 0;
      child = op;
      while ((child = child->getNextSibling()) != NULL) {
	// could be some text between operands, ignore them
	if (child->getNodeType() == DOMNode::TEXT_NODE) {
	  continue;
	} else if (child->getNodeType() == DOMNode::COMMENT_NODE) {
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
	  child = child->getNextSibling();
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

      nodeName = op->getNodeName();
      if (XMLString::equals(nodeName,ADDITION))
	return new Plus(nodes, numChildren);
      if (XMLString::equals(nodeName,SUBTRACTION) && numChildren <= 2) {
	if (numChildren == 1)
	  return new Neg(nodes[0]);
	return new Minus(nodes[0], nodes[1]);
      }
      if (XMLString::equals(nodeName,EXPONENTIATION) && numChildren == 2)
	return new Power(nodes[0], nodes[1]);
      if (XMLString::equals(nodeName,MULTIPLICATION))
	return new Times(nodes, numChildren);
      if (XMLString::equals(nodeName,MAXML))
	return new Max(nodes, numChildren);
      if (XMLString::equals(nodeName,MINML))
	return new Min(nodes, numChildren);
      if (XMLString::equals(nodeName,DIVISION) && numChildren == 2)
	return new Divide(nodes[0], nodes[1]);
      // otherwise, throw an exception and return NULL after the switch
      // switch

      for (int i = 0; i < numChildren; i++)
	delete nodes[i];
      delete[] nodes;
      throw MathMLExprException("Unknown operation");
    }

    if (XMLString::equals(nodeName,NUMBER)) {
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

    if (XMLString::equals(nodeName,IDENTIFIER)) {
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

  case DOMNode::ENTITY_REFERENCE_NODE:
    {
      throw MathMLExprException("Entity -- not supported");
    }
  
  case DOMNode::CDATA_SECTION_NODE:
    {
      throw MathMLExprException("CDATA -- not supported");
    }
  
  case DOMNode::COMMENT_NODE:
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
