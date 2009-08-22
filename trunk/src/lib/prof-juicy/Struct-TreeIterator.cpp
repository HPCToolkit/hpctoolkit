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

//***************************************************************************
//
// File:
//   $HeadURL$
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

//************************* System Include Files ****************************

#include <iostream>
using std::ostream;
using std::endl;


//*************************** User Include Files ****************************

#include "Struct-Tree.hpp"

#include <lib/support/diagnostics.h>

//*************************** Forward Declarations **************************

//***************************************************************************

namespace Prof {

namespace Struct {


//***************************************************************************
// ANodeFilter support
//***************************************************************************

bool
HasANodeTy(const ANode& node, long type)
{
  return (type == ANode::TyANY || node.type() == ANode::IntToANodeTy(type));
}


const ANodeFilter ANodeTyFilter[ANode::TyNUMBER] = {
  ANodeFilter(HasANodeTy, 
	      ANode::ANodeTyToName(ANode::TyRoot).c_str(), ANode::TyRoot),
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
// ANodeIterator
//***************************************************************************


//***************************************************************************
// ANodeSortedIterator
//***************************************************************************

ANodeSortedIterator::
ANodeSortedIterator(const ANode* node,
		    ANodeSortedIterator::cmp_fptr_t compare_fn,
		    const ANodeFilter* filterFunc,
		    bool leavesOnly)
{
  ANodeIterator it(node, filterFunc, leavesOnly);
  for (ANode* cur = NULL; (cur = it.CurNode()); it++) {
    scopes.Add((unsigned long) cur);
  }
  ptrSetIt = new WordSetSortedIterator(&scopes, compare_fn);
}


void
ANodeSortedIterator::DumpAndReset(ostream& os)
{
  os << "ANodeSortedIterator: " << endl;
  while (Current()) {
    os << Current()->ToString() << endl;
    (*this)++;
  } 
  Reset();
}


int
ANodeSortedIterator::cmpByName(const void* a, const void* b)
{
  ANode* x = (*(ANode**)a);
  ANode* y = (*(ANode**)b);
  return x->name().compare(y->name()); // strcmp(x, y)
}


int
ANodeSortedIterator::cmpByLine(const void* a, const void* b)
{
  // WARNING: this assumes it will only see ACodeNodes!
  ACodeNode* x = (*(ACodeNode**)a);
  ACodeNode* y = (*(ACodeNode**)b);
  return ACodeNode::compare(x, y);
}


int
ANodeSortedIterator::cmpById(const void* a, const void* b)
{
  ANode* x = (*(ANode**)a);
  ANode* y = (*(ANode**)b);
  return (x->id() - y->id());
}


int ANodeSortedIterator::cmpByMetric_mId = -1;

int 
ANodeSortedIterator::cmpByMetric_fn(const void* a, const void *b)
{
  ANode* x = *(ANode**) a;
  ANode* y = *(ANode**) b;

  double vx = x->hasMetric(cmpByMetric_mId) ? x->metric(cmpByMetric_mId) : 0.0;
  double vy = y->hasMetric(cmpByMetric_mId) ? y->metric(cmpByMetric_mId) : 0.0;
  double difference = vy - vx;
  
  if (difference < 0) return -1;	
  else if (difference > 0) return 1;	
  return 0;
}


//***************************************************************************
// ANodeSortedChildIterator
//***************************************************************************

ANodeSortedChildIterator::
ANodeSortedChildIterator(const ANode* node,
			 ANodeSortedIterator::cmp_fptr_t compare_fn,
			 const ANodeFilter* f)
{
  ANodeChildIterator it(node, f);
  for (ANode* cur = NULL; (cur = it.CurNode()); it++) {
    scopes.Add((unsigned long) cur);
  }
  ptrSetIt = new WordSetSortedIterator(&scopes, compare_fn);
}


void
ANodeSortedChildIterator::DumpAndReset(ostream& os)
{
  os << "ANodeSortedChildIterator: " << endl;
  while (Current()) {
    os << Current()->ToString() << endl;
    (*this)++;
  }
  Reset();
}

//***************************************************************************

} // namespace Struct

} // namespace Prof
