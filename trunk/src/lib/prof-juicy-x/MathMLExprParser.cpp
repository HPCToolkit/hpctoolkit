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

//************************ System Include Files ******************************

#include <iostream> 
using std::cerr;
using std::endl;

#include <sstream>

#include <string> 

#ifdef NO_STD_CHEADERS
# include <stdlib.h>
#else
# include <cstdlib>
using std::atof; // For compatibility with non-std C headers
using std::atoi;
#endif

//************************* User Include Files *******************************

#include "MathMLExprParser.hpp"

#include <lib/prof-juicy-x/XercesUtil.hpp>

#include <lib/prof-juicy/PerfMetric.hpp>

#include <lib/support/NaN.h>
#include <lib/support/Trace.hpp>
#include <lib/support/StrUtil.hpp>

//************************ Xerces Include Files ******************************

#include <xercesc/util/XMLString.hpp>         
using XERCES_CPP_NAMESPACE::XMLString;


//************************ Forward Declarations ******************************

static bool isOperatorNode(DOMNode *node);
static bool isOperandNode(DOMNode *node);

//****************************************************************************


//****************************************************************************
//
//****************************************************************************

MathMLExprParser::MathMLExprParser()
{
}


MathMLExprParser::~MathMLExprParser()
{
}


EvalNode* 
MathMLExprParser::parse(DOMNode* mathMLExpr, 
			const Prof::MetricDescMgr& mMgr)
{
  EvalNode* exprTree = NULL;

  int exprs = 0;

  DOMNode *child = mathMLExpr->getFirstChild();
  for (; child != NULL; child = child->getNextSibling()) {
    if (child->getNodeType() == DOMNode::TEXT_NODE) {
      // DTD ensures this can't contain anything but white space
      continue;
    } 
    else if (child->getNodeType() == DOMNode::COMMENT_NODE) {
      continue;
    }
    
    if (exprs == 1) {
      throw MathMLExprException("More than one MathML expression specified for COMPUTE");
    }
    IFTRACE << child->getNodeType() << endl; 
    exprs++;
    exprTree = buildEvalTree(child, mMgr);
  }
  if (exprs == 0) {
    throw MathMLExprException("No MathML expression specified for COMPUTE");
  }

  return exprTree;
}


// ----------------------------------------------------------------------
// -- EvalNode* buildEvalTree(DOMNode *node) --
//   Build an EvalNode object out of a DOMNode object.
// -- params --
//   node:    a DOMNode
// -- exception --
//   An exception of type MathMLExprException could be thrown.  If the 
// ----------------------------------------------------------------------

EvalNode* 
MathMLExprParser::buildEvalTree(DOMNode *node, 
				const Prof::MetricDescMgr& mMgr) 
{
  bool isNumber;

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
	std::string str = make_string(nodeValue);
	IFTRACE << "str --" << str << "--" << endl; 
	double val = StrUtil::toDbl(str.c_str());
	evalNode = new Const(val);
	IFTRACE << "number is --" << val << "--" << endl; 
      }
      else {           // is a variable
	std::string str = make_string(nodeValue);
	IFTRACE << "str --" << str << "--" << endl; 

	const PerfMetric* m = mMgr.metric(str);
	if (!m) {
	  MathML_Throw("Undefined metric '" + str + "' encountered");
	}

	IFTRACE << "index --" << m->Index() << "--" << endl;
	evalNode = new Var(str, m->Index());
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
	  nodes[i] = buildEvalTree(child, mMgr);
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
	builtNode = buildEvalTree(child, mMgr);
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
	builtNode = buildEvalTree(child, mMgr);
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


#if 0
std::string
MathMLExprParser::toString() const
{
  std::ostringstream os;
  dump(os);
  return os.str();
}


std::ostream&
MathMLExprParser::dump(std::ostream& os) const
{ 
  return os;
}
#endif


// ----------------------------------------------------------------------
// -- bool isOperatorNode(DOMNode *node) --
//   Tests whether a DOMNode is one of the operator nodes -- divide,
//   minus, plus, power and times.
// -- params --
//   node:       a DOMNode
// ----------------------------------------------------------------------

static bool 
isOperatorNode(DOMNode *node) 
{  
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

static bool 
isOperandNode(DOMNode *node) 
{
  static const XMLCh* APPLY = XMLString::transcode("apply");
  static const XMLCh* NUMBER = XMLString::transcode("cn");
  static const XMLCh* IDENTIFIER = XMLString::transcode("ci");

  const XMLCh* nodeName = node->getNodeName();

  return (XMLString::equals(nodeName,APPLY) ||
	  XMLString::equals(nodeName,NUMBER) || XMLString::equals(nodeName,IDENTIFIER));
}

