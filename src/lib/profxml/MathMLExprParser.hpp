// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2016, Rice University
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
//   header file for the implementation of arithmetic evaluation of
//   metrics
//
// ----------------------------------------------------------------------

#ifndef profxml_MathMLExprParser_hpp
#define profxml_MathMLExprParser_hpp

//************************ System Include Files ******************************

#include <iostream>
#include <string>

//************************ Xerces Include Files ******************************

#include <xercesc/dom/DOMNode.hpp>
using XERCES_CPP_NAMESPACE::DOMNode;

//************************* User Include Files *******************************

#include <lib/prof/Metric-AExpr.hpp>
#include <lib/prof/Metric-Mgr.hpp>

//************************ Forward Declarations ******************************

//****************************************************************************


// ----------------------------------------------------------------------
//
// class MathMLExprParser
//   The expressive power of this MathMLExprParser class is determined by the
//   kinds of nodes supported by class Prof::Metric::AExpr.  Only the content
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

class MathMLExprParser
{
public:

  MathMLExprParser();
  ~MathMLExprParser();

  // ------------------------------------------------------------
  //   Build a new MathMLExprParser object out of the specified input string.
  //   This string contains MathML expressions.
  // -- params --
  //   inputSource:    input string in XMLCh format
  // -- exception --
  //   MathMLExprParserException could be thrown due to invalid or unsupported
  //   MathML expressions.
  // ------------------------------------------------------------
  static Prof::Metric::AExpr* 
  parse(DOMNode* mathMLExpr, 
	const Prof::Metric::Mgr& mMgr);
  
  // ------------------------------------------------------------
  //
  // ------------------------------------------------------------
#if 0
  std::string toString() const;
  std::ostream& dump(std::ostream& os = std::cout) const;
#endif

private:
  static Prof::Metric::AExpr* 
  buildEvalTree(DOMNode *node,
		const Prof::Metric::Mgr& mMgr,
		bool isNum);
};



// ----------------------------------------------------------------------
// class MathMLException
//   Exceptions thrown during the construction of MathMLExpr object or
//   evaluation of MathMLExpr
// ----------------------------------------------------------------------

#define MathML_Throw(streamArgs) DIAG_ThrowX(MathMLExprException, streamArgs)

class MathMLExprException : public Diagnostics::Exception {
public:
  MathMLExprException(const std::string x,
		      const char* filenm = NULL, unsigned int lineno = 0)
    : Diagnostics::Exception(x, filenm, lineno)
    { }

  virtual std::string message() const { 
    return "Math ML Exception: " + what();
  }
  
private:
};



#endif /* profxml_MathMLExprParser_hpp */
