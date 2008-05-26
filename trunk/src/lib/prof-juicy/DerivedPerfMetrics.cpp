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
using std::endl;

#include <string>
using std::string;

//************************* User Include Files *******************************

#include "DerivedPerfMetrics.hpp"

#include <lib/prof-juicy/PgmScopeTreeInterface.hpp>
#include <lib/prof-juicy/PgmScopeTree.hpp>

#include <lib/support/diagnostics.h>
#include <lib/support/pathfind.h>
#include <lib/support/NaN.h>
#include <lib/support/StrUtil.hpp>
#include <lib/support/Trace.hpp>

//************************ Forward Declarations ******************************

//****************************************************************************

// *************************************************************************
// classes derived from PerfMetric
// *************************************************************************

FilePerfMetric::FilePerfMetric(const char* nm, 
			       const char* nativeNm,
			       const char* displayNm, 
			       bool doDisp, bool doPerc, bool doSort, 
			       const char* file,
			       const char* type,
			       bool isunit_ev) 
  : PerfMetric(nm, nativeNm, displayNm, doDisp, doPerc, false, doSort),
    m_file(file), m_type(type), m_isunit_event(isunit_ev)
{ 
  // trace = 1;
}


FilePerfMetric::FilePerfMetric(const std::string& nm, 
			       const std::string& nativeNm, 
			       const std::string& displayNm,
			       bool doDisp, bool doPerc, bool doSort, 
			       const std::string& file, 
			       const std::string& type,
			       bool isunit_ev)
  : PerfMetric(nm, nativeNm, displayNm, doDisp, doPerc, false, doSort),
    m_file(file), m_type(type), m_isunit_event(isunit_ev)
{ 
  // trace = 1;
}


FilePerfMetric::~FilePerfMetric() 
{
  IFTRACE << "~FilePerfMetric " << toString() << endl; 
}


void 
FilePerfMetric::Make(NodeRetriever &ret) const
{
  IFTRACE << "FilePerfMetric::Make " << endl << " " << toString() << endl;
  DIAG_Assert(m_type == "HPCRUN", "Unexpected FilePefMetric type: " << m_type);
  // handled within the Driver
}


string
FilePerfMetric::toString(int flags) const 
{
  string unitstr = isunit_event() ? " [events]" : " [samples]";
  string rawstr = " {" + m_rawdesc.description() + ":" + StrUtil::toStr(m_rawdesc.period()) + " ev/smpl}";
  string str = PerfMetric::toString() + unitstr + rawstr;

  if (flags) {
    str += "file='" + m_file + "' " + "type='" + m_type + "'";
  }
  return str;
} 


// **************************************************************************
// 
// **************************************************************************

ComputedPerfMetric::ComputedPerfMetric(const char* nm, const char* displayNm,
				       bool doDisp, bool doPerc, bool doSort,
				       bool propagateComputed,
				       EvalNode* expr)
  : PerfMetric(nm, "", displayNm, doDisp, doPerc, propagateComputed, doSort)
{
  Ctor(nm, expr);
}


ComputedPerfMetric::ComputedPerfMetric(const std::string& nm,
				       const std::string& displayNm,
				       bool doDisp, bool doPerc, bool doSort,
				       bool propagateComputed,
				       EvalNode* expr)
  : PerfMetric(nm, "", displayNm, doDisp, doPerc, propagateComputed, doSort)
{
  Ctor(nm.c_str(), expr);
}


void
ComputedPerfMetric::Ctor(const char* nm, EvalNode* expr)
{
  m_exprTree = expr;
}


ComputedPerfMetric::~ComputedPerfMetric() 
{
  IFTRACE << "~ComputedPerfMetric " << toString() << endl; 
  delete m_exprTree;
}


void 
ComputedPerfMetric::Make(NodeRetriever& structIF) const
{
  PgmScope* pgmStrct = structIF.GetRoot();
  ScopeInfoIterator it(pgmStrct, NULL /*filter*/, false /*leavesOnly*/,
		       IteratorStack::PostOrder);

  for (; it.Current(); it++) {
    if (it.CurScope()->IsLeaf() || !PropComputed()) {
      double val = c_FP_NAN_d;
      if (m_exprTree) {
	val = m_exprTree->eval(it.CurScope());
      }

      if (! c_isnan_d(val)) {
	it.CurScope()->SetPerfData(Index(), val); 
      } 
    }
  }

  if (PropComputed()) {
    pgmStrct->accumulateMetrics(Index());
  }
}


string
ComputedPerfMetric::toString(int flags) const 
{
  string str = PerfMetric::toString() 
    + "{" + m_exprTree->toString() + "}";
  return str;
} 

