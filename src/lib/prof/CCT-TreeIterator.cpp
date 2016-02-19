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


static int
cmpByDynInfoSpecial(const ADynNode* x_dyn, const ADynNode* y_dyn)
{
  // INVARIANT: x and y are non-NULL

  // 1. lm-id for physical or logical frames
  int cmp_lmId = x_dyn->lmId() - y_dyn->lmId();
  if (cmp_lmId != 0) {
    return cmp_lmId;
  }

  // 2. physical ip
  int cmp_ip_real = cmp(x_dyn->lmIP_real(), y_dyn->lmIP_real());
  if (cmp_ip_real != 0) {
    return cmp_ip_real;
  }

  // 3. logical ip (if available)
  int cmp_ip = cmp(x_dyn->lmIP(), y_dyn->lmIP());
  return cmp_ip;
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
    // 0. test for equality
    if (x == y) {
      return 0;
    }

    // INVARIANT: x != y, so never return 0
    
    // 1. distinguish by structure ids
    uint x_id = x->structureId();
    uint y_id = y->structureId();
    int cmp_sid = cmp(x_id, y_id);
    if (cmp_sid != 0) {
      return cmp_sid;
    }

    // 2. distinguish by types
    int cmp_ty = (int)x->type() - (int)y->type();
    if (cmp_ty != 0) {
      return cmp_ty;
    }

    // 3. distinguish by dynamic info (unnormalized CCTs)
    //    (for determinism, ensure x and y are both ADynNodes)
    ADynNode* x_dyn = dynamic_cast<ADynNode*>(x);
    ADynNode* y_dyn = dynamic_cast<ADynNode*>(y);
    if (x_dyn && y_dyn) {
      int cmp_dyn = cmpByDynInfoSpecial(x_dyn, y_dyn);
      if (cmp_dyn != 0) {
	return cmp_dyn;
      }
    }
    
    
    // 5. distinguish by id
    int cmp_id = (int)x->id() - (int)y->id();
    if (cmp_id != 0) {
      return cmp_id;
    }

    // 4. distinguish by tree context
    ANode* x_parent = x->parent();
    ANode* y_parent = y->parent();
    if (x_parent != y_parent) {
      int cmp_ctxt = cmpByStructureInfo(&x_parent, &y_parent);
      if (cmp_ctxt != 0) {
	return cmp_ctxt;
      }
    }
    // *. Could compare childCount() and other aspects of children.
    DIAG_Die("Prof::CCT::ANodeSortedIterator::cmpByStructureInfo: cannot compare:"
		<< "\n\tx: " << x->toStringMe(Prof::CCT::Tree::OFlg_Debug)
		<< "\n\ty: " << y->toStringMe(Prof::CCT::Tree::OFlg_Debug));
    return 0;
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

  // 0. test for equality
  if (x == y) {
    return 0;
  }

  // INVARIANT: x != y, so never return 0

  ADynNode* x_dyn = dynamic_cast<ADynNode*>(x);
  ADynNode* y_dyn = dynamic_cast<ADynNode*>(y);

  // 1. distinguish by dynamic info
  if (x_dyn && y_dyn) {
    return cmpByDynInfoSpecial(x_dyn, y_dyn);
  }

  // 2. distinguish by structure ids
  uint x_id = x->structureId();
  uint y_id = y->structureId();
  if (x_id != Prof::Struct::ANode::Id_NULL
      && y_id != Prof::Struct::ANode::Id_NULL) {
    int cmp_sid = cmp(x_id, y_id);
    if (cmp_sid != 0) {
      return cmp_sid;
    }
  }

  // 3. distinguish by type
  int cmp_ty = (int)x->type() - (int)y->type();
  if (cmp_ty != 0) {
    return cmp_ty;
  }


#if 1
  // 4. distinguish by id
  int cmp_id = (int)x->id() - (int)y->id();
  if (cmp_id != 0) {
    return cmp_id;
  }
#endif

  // *. Could compare childCount() and other aspects of children.
  DIAG_Die("Prof::CCT::ANodeSortedIterator::cmpByDynInfo: cannot compare:"
	   << "\n\tx: " << x->toStringMe(Prof::CCT::Tree::OFlg_Debug)
	   << "\n\ty: " << y->toStringMe(Prof::CCT::Tree::OFlg_Debug));
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
