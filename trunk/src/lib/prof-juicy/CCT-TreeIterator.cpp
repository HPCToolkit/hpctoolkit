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
// Copyright ((c)) 2002-2010, Rice University 
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

static int
cmp(uint64_t x, uint64_t y)
{
  // compare without the possible overflow caused by (x - y)
  if (x == y) {
    return 0;
  }
  else if (x < y) {
    return -1;
  }
  else {
    return 1;
  }
}


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
    os << current()->toStringMe() << endl;
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
ANodeSortedIterator::cmpByStructureInfo(const void* a, const void* b)
{
  ANode* x = (*(ANode**)a);
  ANode* y = (*(ANode**)b);

  if (x && y) {
    uint x_id = x->structureId();
    uint y_id = y->structureId();
    
    int cmp_id = cmp(x_id, y_id);
    int cmp_ty = (int)x->type() - (int)y->type();
    if (cmp_id == 0 && cmp_ty == 0 && x != y) {
      // hard case: cannot return 0!
      ANode* x_parent = x->parent();
      ANode* y_parent = y->parent();
      if (x_parent == y_parent) {
	// This should be sufficient for the CCTs that we see.
	// Could compare childCount() and other aspects of children.
	return cmpByDynInfo(a, b);
      }
      else {
	// if parents are different, compare by context
	ANode* x_parent = x->parent();
	ANode* y_parent = y->parent();
	return cmpByStructureInfo(&x_parent, &y_parent);
      }
    }
    else if (cmp_id == 0) {
      return cmp_ty;
    }
    else {
      return cmp_id;
    }
  }
  else if (x) {
    return 1; // x > y=NULL (only used for recursive case)
  }
  else if (y) {
    return -1; // x=NULL < y (only used for recursive case)
  }
  else {
    DIAG_Die(DIAG_UnexpectedInput);
  }
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
    return cmp(x_dyn->ip(), y_dyn->ip());
  }
  else if (x_dyn) {
    return 1; // x_dyn > y_dyn=NULL
  }
  else if (y_dyn) {
    return -1; // x_dyn=NULL < y_dyn
  }
  else {
    return cmp((int64_t)x, (int64_t)y);
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
    os << current()->toStringMe() << endl;
    (*this)++;
  }
  reset();
}

//***************************************************************************

} // namespace CCT

} // namespace Prof
