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

#include <stdint.h>

//*************************** User Include Files ****************************

#include "CCT-Tree.hpp"

#include <lib/support/diagnostics.h>

//*************************** Forward Declarations **************************

//***************************************************************************

namespace Prof {

namespace CCT {


//***************************************************************************
// ANodeFilter support
//***************************************************************************

bool
HasANodeTy(const ANode& node, long type)
{
  return (type == ANode::TyANY || node.type() == ANode::IntToANodeType(type));
}


const ANodeFilter ANodeTyFilter[ANode::TyNUMBER] = {
  ANodeFilter(HasANodeTy, ANode::ANodeTyToName(ANode::TyRoot).c_str(),
	      ANode::TyRoot),
  ANodeFilter(HasANodeTy, ANode::ANodeTyToName(ANode::TyProcFrm).c_str(),
	      ANode::TyProcFrm),
  ANodeFilter(HasANodeTy, ANode::ANodeTyToName(ANode::TyProc).c_str(),
	      ANode::TyProc),
  ANodeFilter(HasANodeTy, ANode::ANodeTyToName(ANode::TyLoop).c_str(),
	      ANode::TyLoop),
  ANodeFilter(HasANodeTy, ANode::ANodeTyToName(ANode::TyStmt).c_str(),
	      ANode::TyStmt),
  ANodeFilter(HasANodeTy, ANode::ANodeTyToName(ANode::TyCall).c_str(),
	      ANode::TyCall),
  ANodeFilter(HasANodeTy, ANode::ANodeTyToName(ANode::TyANY).c_str(),
	      ANode::TyANY)
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
  for (ANode* cur = NULL; (cur = it.current()); it++) {
    scopes.Add((unsigned long) cur);
  }
  ptrSetIt = new WordSetSortedIterator(&scopes, compare_fn);
}


void
ANodeSortedIterator::dumpAndReset(ostream& os)
{
  os << "ANodeSortedIterator: " << endl;
  while (current()) {
    os << current()->toString_me() << endl;
    (*this)++;
  } 
  reset();
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
  ANode* x = (*(ANode**)a);
  ANode* y = (*(ANode**)b);
  return ANodeLineComp(x, y);
}


int
ANodeSortedIterator::cmpByStructureId(const void* a, const void* b)
{
  ANode* x = (*(ANode**)a);
  ANode* y = (*(ANode**)b);
  uint x_id = x->structureId();
  uint y_id = y->structureId();
  return (x_id - y_id);
}


int
ANodeSortedIterator::cmpByDynInfo(const void* a, const void* b)
{
  ANode* x = (*(ANode**)a);
  ANode* y = (*(ANode**)b);

  ADynNode* x_dyn = dynamic_cast<ADynNode*>(x);
  ADynNode* y_dyn = dynamic_cast<ADynNode*>(y);

  if (x_dyn && y_dyn) {
    int diff_lmId = x_dyn->lmId() - y_dyn->lmId();
    if (diff_lmId != 0) {
      return diff_lmId;
    }
    intptr_t diff_ip = (intptr_t)(x_dyn->ip() - y_dyn->ip());
    if (diff_ip == 0)     { return 0; }
    else if (diff_ip < 0) { return -1; }
    else                  { return 1; }
  }
  else if (x_dyn) {
    return -1;
  }
  else if (y_dyn) {
    return 1;
  }
  else {
    return 0;
  }
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
  for (ANode* cur = NULL; (cur = it.current()); it++) {
    scopes.Add((unsigned long) cur);
  }
  ptrSetIt = new WordSetSortedIterator(&scopes, compare_fn);
}


void
ANodeSortedChildIterator::dumpAndReset(ostream& os)
{
  os << "ANodeSortedChildIterator: " << endl;
  while (current()) {
    os << current()->toString_me() << endl;
    (*this)++;
  }
  reset();
}

//***************************************************************************

} // namespace CCT

} // namespace Prof
