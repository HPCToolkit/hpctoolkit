// -*-C++-*-
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

//************************ System Include Files ******************************

#include <iostream>
using std::cout;
using std::cerr;
using std::endl;

#include <string>
using std::string;

//************************* User Include Files *******************************

#include "HPCViewSAX2.hpp"
#include "DerivedPerfMetrics.hpp"
#include "NodeRetriever.hpp"
#include "PROFILEDocHandler.hpp"
#include "MathMLExpr.hpp"

#include <lib/prof-juicy/PgmScopeTree.hpp>

#include <lib/support/Trace.hpp>
#include <lib/support/pathfind.h>
#include <lib/support/StrUtil.hpp>

//************************ Forward Declarations ******************************

//****************************************************************************

// *************************************************************************
// classes derived from PerfMetric
// *************************************************************************

FilePerfMetric::FilePerfMetric(const char* nm, const char* nativeNm,
			       const char* displayNm, bool doDisp, 
			       bool doPerc, bool doSort, const char* fName,
			       const char* tp, Driver* _driver) 
  : PerfMetric(nm, nativeNm, displayNm, doDisp, doPerc, false, doSort),
    file(fName), type(tp), driver(_driver)
{ 
  // trace = 1;
}

FilePerfMetric::~FilePerfMetric() 
{
  IFTRACE << "~FilePerfMetric " << ToString() << endl; 
}

ComputedPerfMetric::ComputedPerfMetric(const char* nm, const char* displayNm, 
				       bool doDisp, bool doPerc, bool doSort, 
				       bool propagateComputed, 
				       DOMNode *expr)
  : PerfMetric(nm, NULL, displayNm, doDisp, doPerc, propagateComputed, doSort)
{
   try {
     mathExpr = new MathMLExpr(expr); 
   }
   catch (const MathMLExprException &e) {
     cerr << "hpcview fatal error: Could not construct METRIC '" << nm << "'." << endl 
	  << "\tXML exception encountered when processing MathML expression: " 
	  << e.getMessage() << "." << endl;
     exit(1);
   }
   catch (...) {
     cerr << "hpcview fatal error: Could not construct metric " << endl 
	  << "\tUnknown exception encountered handling MathML expression." << endl; 
     exit(1);
   }
   if (mathExpr != NULL) { // catch exception really 
     cout << "Computed METRIC " << nm << ": "
	  << nm << " = "; mathExpr->print(); cout << endl;
   } 
}

ComputedPerfMetric::~ComputedPerfMetric() 
{
  IFTRACE << "~ComputedPerfMetric " << ToString() << endl; 
  delete mathExpr; 
}

// **************************************************************************
// Make methods 
// **************************************************************************

static void
AccumulateFromChildren(ScopeInfo &si, int perfInfoIndex) 
{
  ScopeInfoChildIterator it(&si); 
  for (; it.Current(); it++) { 
     AccumulateFromChildren(*it.CurScope(), perfInfoIndex); 
  } 
  it.Reset(); 
  if (it.Current() != NULL) { // its not a leaf 
    double val = 0.0; 
    bool hasVal = false; 
    for (; it.Current(); it++) { 
      if (it.CurScope()->HasPerfData(perfInfoIndex)) { 
         val += it.CurScope()->PerfData(perfInfoIndex); 
	 hasVal = true; 
      }
    } 
    if (hasVal) { 
      si.SetPerfData(perfInfoIndex, val); 
    }
  }
}

void FilePerfMetric::Make(NodeRetriever &ret)
{
  IFTRACE << "FilePerfMetric::Make " << endl << " " << ToString() << endl;
  
  SAX2XMLReader* parser = XMLReaderFactory::createXMLReader();

  parser->setFeature(XMLUni::fgSAX2CoreValidation, true);
  // parser->setFeature(XMLUni::fgXercesDynamic, true);

  PROFILEDocHandler handler(&ret, driver);
  parser->setContentHandler(&handler);
  parser->setErrorHandler(&handler); 

  const char* filePath = pathfind(".", file.c_str(), "r");
  if (!filePath) {
    cerr << "hpcview fatal error: could not open PROFILE file '" 
	 << file << "'." << endl;
    exit(1);
  }
  
  try {
    handler.Initialize(Index(), filePath);
    parser->parse(filePath);
  }
  catch (const PROFILEException& toCatch) {
    string msg = toCatch.message();
    throw MetricException(msg); 
  }
  delete parser;
  
  AccumulateFromChildren(*ret.GetRoot(), Index());
  
  if (!ret.GetRoot()->HasPerfData(Index())) {
    // eraxxon: Instead of throwning an exception, let's emit a warning.
    string msg = "File '" + file + 
      "' does not contain any information for metric '" + Name() + "'";
    //throw MetricException(msg);
    cerr << "hpcview warning: " << msg << endl;
  }
  IFTRACE << "FilePerfMetric::Make yields: " << ToString() << endl;
}

void ComputedPerfMetric::Make(NodeRetriever &ret)
{
  ScopeInfoIterator it(ret.GetRoot(), 
		       /*filter*/ NULL, /*leavesOnly*/ false, PostOrder); 

  for (; it.Current(); it++) {
    if (it.CurScope()->IsLeaf() 
	|| !IndexToPerfDataInfo(Index()).PropComputed()) {
      double val = mathExpr->eval(it.CurScope()); 
      if (! IsNaN(val)) {
	it.CurScope()->SetPerfData(Index(), val); 
      } 
    }
  }

  if (IndexToPerfDataInfo(Index()).PropComputed()) {
    AccumulateFromChildren(*ret.GetRoot(), Index());
  }
}

// **************************************************************************
// ToString methods 
// **************************************************************************
string
FilePerfMetric::ToString() const 
{
  return PerfMetric::ToString() + " " +  string("FilePerfMetric: " ) + 
         "file=\"" + file + "\" " + 
         "type=\"" + type + "\""; 
} 

string
ComputedPerfMetric::ToString() const 
{
  return PerfMetric::ToString() + " " + string("ComputeMetricInfo: " ) + 
         "MathMLExpr=\"" + StrUtil::toStr((void*)mathExpr);
} 

