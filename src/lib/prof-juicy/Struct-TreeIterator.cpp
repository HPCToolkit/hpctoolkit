// -*-Mode: C++;-*-
// $id: PgmScopeTreeIterator.cpp 580 2006-07-27 14:16:10Z eraxxon $

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

//***************************************************************************
//
// File:
//    $Source$
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

//************************* System Include Files ****************************

#include <iostream>
using std::ostream;
using std::cerr;
using std::endl;

#include <vector>

//*************************** User Include Files ****************************

#include "Struct-Tree.hpp"
#include "PerfMetric.hpp"

#include <lib/support/diagnostics.h>

//*************************** Forward Declarations **************************


namespace Prof {
namespace Struct {
  static int CompareByLine(const void *a, const void *b);
} // namespace Struct
} // namespace Prof


//***************************************************************************

namespace Prof {
namespace Struct {

//***************************************************************************
// ANodeFilter support
//***************************************************************************

bool 
HasANodeTy(const ANode& node, long type)
{
  return (type == ANode::TyANY
	  || node.Type() == ANode::IntToANodeTy(type));
}

const ANodeFilter ANodeTyFilter[ANode::TyNUMBER] = {
  ANodeFilter(HasANodeTy, 
	      ANode::ANodeTyToName(ANode::TyPGM).c_str(), ANode::TyPGM),
  ANodeFilter(HasANodeTy, 
	      ANode::ANodeTyToName(ANode::TyGROUP).c_str(), ANode::TyGROUP),
  ANodeFilter(HasANodeTy,
	      ANode::ANodeTyToName(ANode::TyLM).c_str(), ANode::TyLM),
  ANodeFilter(HasANodeTy,
	      ANode::ANodeTyToName(ANode::TyFILE).c_str(), ANode::TyFILE),
  ANodeFilter(HasANodeTy,
	      ANode::ANodeTyToName(ANode::TyPROC).c_str(), ANode::TyPROC),
  ANodeFilter(HasANodeTy,
	      ANode::ANodeTyToName(ANode::TyALIEN).c_str(), ANode::TyALIEN),
  ANodeFilter(HasANodeTy,
	      ANode::ANodeTyToName(ANode::TyLOOP).c_str(), ANode::TyLOOP),
  ANodeFilter(HasANodeTy,
	      ANode::ANodeTyToName(ANode::TySTMT).c_str(), ANode::TySTMT),
  ANodeFilter(HasANodeTy,
	      ANode::ANodeTyToName(ANode::TyREF).c_str(), ANode::TyREF),
  ANodeFilter(HasANodeTy,
	      ANode::ANodeTyToName(ANode::TyANY).c_str(), ANode::TyANY)
};
  
  
//***************************************************************************
// ANodeChildIterator
//***************************************************************************

//***************************************************************************
// ACodeNodeChildIterator
//***************************************************************************

//***************************************************************************
// ANodeIterator
//***************************************************************************

//***************************************************************************
// ANodeLineSortedIterator
//***************************************************************************

ANodeLineSortedIterator::
ANodeLineSortedIterator(const ACodeNode *file, 
			    const ANodeFilter *filterFunc, 
			    bool leavesOnly)
{
  ANodeIterator it(file, filterFunc, leavesOnly);
  ANode *cur;
  for (; (cur = it.CurNode()); ) {
    scopes.Add((unsigned long) cur);
    it++;
  }
  ptrSetIt = new WordSetSortedIterator(&scopes, CompareByLine);
}


ANodeLineSortedIterator::~ANodeLineSortedIterator() 
{
  delete ptrSetIt;
} 


void 
ANodeLineSortedIterator::DumpAndReset(ostream &os)
{
  os << "ANodeLineSortedIterator: " << endl;
  while (Current()) {
    os << Current()->ToString() << endl;
    (*this)++;
  } 
  Reset();
}


static int 
CompareByLine(const void* a, const void *b) 
{
  ACodeNode* x = (*(ACodeNode**)a);
  ACodeNode* y = (*(ACodeNode**)b);
  DIAG_Assert (x != NULL, "");
  DIAG_Assert (y != NULL, "");
  return ACodeNodeLineComp(x, y);
}

//***************************************************************************
// ANodeLineSortedChildIterator
//***************************************************************************

ANodeLineSortedChildIterator::
ANodeLineSortedChildIterator(const ANode *scope, 
				 const ANodeFilter * f)
{
  ANodeChildIterator it(scope, f);
  ANode *cur;
  for (; (cur = it.CurNode()); ) {
    scopes.Add((unsigned long) cur);
    it++;
  }
  ptrSetIt = new WordSetSortedIterator(&scopes, CompareByLine);
}


ANodeLineSortedChildIterator::~ANodeLineSortedChildIterator()
{
  delete ptrSetIt;
}


void
ANodeLineSortedChildIterator::DumpAndReset(ostream &os)
{
  os << "ANodeLineSortedChildIterator: " << endl;
  while (Current()) {
    os << Current()->ToString() << endl;
    (*this)++;
  }
  Reset();
}


//***************************************************************************
// ANodeLineSortedIteratorForLargeScopes
//***************************************************************************

ANodeLineSortedIteratorForLargeScopes::ANodeLineSortedIteratorForLargeScopes(
					const ACodeNode *file, 
					const ANodeFilter *filterFunc, 
					bool leavesOnly)
{
  ANodeIterator it(file, filterFunc, leavesOnly);
  ANode *cur;
  for (; (cur = it.CurNode()); ) {
    ACodeNodeLine *cur1 = new ACodeNodeLine(dynamic_cast<ACodeNode*>(cur), 
                         IS_BEG_LINE);
    scopes.Add((unsigned long) cur1);
    if (cur->Type() == ANode::TyLOOP)
    {  // create a ACodeNodeLine object for both begin and end line
       // only for Loops. For PROCs we are interested only in BegLine
      ACodeNodeLine *cur2 = new ACodeNodeLine(dynamic_cast<ACodeNode*>(cur), 
                         IS_END_LINE);
      scopes.Add((unsigned long) cur2);
    }
    it++;
  }
  ptrSetIt = new WordSetSortedIterator(&scopes, CompareByLine);
}


ANodeLineSortedIteratorForLargeScopes::~ANodeLineSortedIteratorForLargeScopes() 
{
  // First deallocate all ACodeNodeLine objects created
  ptrSetIt->Reset();
  for ( ; ptrSetIt->Current() ; (*ptrSetIt)++ )
    delete ((ACodeNodeLine*) (*ptrSetIt->Current()));
  delete ptrSetIt;
}
 
ACodeNodeLine* 
ANodeLineSortedIteratorForLargeScopes::Current() const
{
  ACodeNodeLine *cur = NULL;
  if (ptrSetIt->Current()) {
    cur = (ACodeNodeLine*) (*ptrSetIt->Current());
    DIAG_Assert(cur != NULL, "");
  }
  return cur;
} 

void 
ANodeLineSortedIteratorForLargeScopes::DumpAndReset(std::ostream &os)
{
  os << "ANodeLineSortedIteratorForLargeScopes: " << endl;
  while (Current()) {
    os << Current()->GetACodeNode()->ToString() << endl;
    (*this)++;
  } 
  Reset();
}

void 
ANodeLineSortedIteratorForLargeScopes::Reset()
{
  ptrSetIt->Reset();
}

//***************************************************************************
// ANodeNameSortedChildIterator
//***************************************************************************

ANodeNameSortedChildIterator::
ANodeNameSortedChildIterator(const ANode *scope, 
				 const ANodeFilter * f)
{
  ANodeChildIterator it(scope, f);
  ANode *cur;
  for (; (cur = it.CurNode()); ) {
    scopes.Add((unsigned long) cur);
    it++;
  }
  ptrSetIt = new WordSetSortedIterator(&scopes, CompareByName);
}

ANodeNameSortedChildIterator::~ANodeNameSortedChildIterator() 
{
  delete ptrSetIt;
}
 
ACodeNode* 
ANodeNameSortedChildIterator::Current() const
{
  ACodeNode *cur = NULL;
  if (ptrSetIt->Current()) {
    cur = (ACodeNode*) (*ptrSetIt->Current());
    DIAG_Assert(cur != NULL, "");
  }
  return cur;
}

void 
ANodeNameSortedChildIterator::Reset()
{
  ptrSetIt->Reset();
}

int 
ANodeNameSortedChildIterator::CompareByName(const void* a, const void *b) 
{
  ANode* x = (*(ANode**)a);
  ANode* y = (*(ANode**)b);
  DIAG_Assert (x != NULL, "");
  DIAG_Assert (y != NULL, "");
  return x->name().compare(y->name());
}

//***************************************************************************
// ANodeMetricSortedIterator
//***************************************************************************

ANodeMetricSortedIterator::ANodeMetricSortedIterator(const Pgm* pgm,
						     uint metricId,
						     const ANodeFilter* filterFunc)
  : m_metricId(metricId)
{
  DIAG_Assert(pgm != NULL, "");

  ANodeIterator it(pgm, filterFunc, false);
  ANode *cur;
  for (; (cur = it.CurNode()); ) {
    scopes.Add((unsigned long) cur);
    it++;
  }
  CompareByMetricId_mId = m_metricId;
  ptrSetIt = new WordSetSortedIterator(&scopes, CompareByMetricId);
}



//***************************************************************************
// ANodeMetricSortedChildIterator
//***************************************************************************

ANodeMetricSortedChildIterator::ANodeMetricSortedChildIterator
(const ANode *scope, int flattenDepth, 
 int compare(const void* a, const void *b),
 const ANodeFilter *filterFunc)
{
  depth = flattenDepth;
  AddChildren(scope, 0, filterFunc);
  ptrSetIt = new WordSetSortedIterator(&scopes, compare);
}


void ANodeMetricSortedChildIterator::AddChildren
(const ANode *scope, int curDepth, const ANodeFilter *filterFunc)
{
  DIAG_Assert(scope != NULL, "");
  
  ANodeChildIterator it(scope, filterFunc);
  ANode *cur;
  for (; (cur = it.CurNode()); ) {
    if ((curDepth == depth) || cur->IsLeaf()) {
      scopes.Add((unsigned long) cur);
    } else {
      AddChildren(cur, curDepth+1, filterFunc);
    }
    it++;
  }
}

//***************************************************************************
// static functions
//***************************************************************************

int CompareByMetricId_mId = -1;

int CompareByMetricId(const void* a, const void *b)
{
  ANode* x = *(ANode**) a;
  ANode* y = *(ANode**) b;
  DIAG_Assert(x != NULL, "");
  DIAG_Assert(y != NULL, "");
  double vx = 0.0;
  if (x->HasPerfData(CompareByMetricId_mId)) {
    vx = x->PerfData(CompareByMetricId_mId);
  }
  double vy = 0.0;
  if (y->HasPerfData(CompareByMetricId_mId)) {
    vy = y->PerfData(CompareByMetricId_mId);
  }
  double difference = vy - vx;
  
  if (difference < 0) return -1;	
  else if (difference > 0) return 1;	
  return 0;
}


} // namespace Struct
} // namespace Prof
