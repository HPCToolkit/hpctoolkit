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

//***************************************************************************
//
// File:
//   $Source$
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

#include "CCT-Tree.hpp"

#include <lib/support/diagnostics.h>

//*************************** Forward Declarations ***************************

//****************************************************************************

namespace Prof {

namespace CCT {


static int CompareByLine(const void *a, const void *b);


//*****************************************************************************
// ANodeFilter support
//*****************************************************************************

bool HasNodeType(const ANode& sinfo, long type)
{
  return (type == ANode::TyANY) || (sinfo.type() == ANode::IntToNodeType(type)); 
}


const ANodeFilter NodeTypeFilter[ANode::TyNUMBER] = {
  ANodeFilter(HasNodeType, ANode::NodeTypeToName(ANode::TyRoot).c_str(),
	      ANode::TyRoot),
  ANodeFilter(HasNodeType, ANode::NodeTypeToName(ANode::TyProcFrm).c_str(),
	      ANode::TyProcFrm),
  ANodeFilter(HasNodeType, ANode::NodeTypeToName(ANode::TyProc).c_str(),
	      ANode::TyProc),
  ANodeFilter(HasNodeType, ANode::NodeTypeToName(ANode::TyLoop).c_str(),
	      ANode::TyLoop),
  ANodeFilter(HasNodeType, ANode::NodeTypeToName(ANode::TyStmt).c_str(),
	      ANode::TyStmt),
  ANodeFilter(HasNodeType, ANode::NodeTypeToName(ANode::TyCall).c_str(),
	      ANode::TyCall),
  ANodeFilter(HasNodeType, ANode::NodeTypeToName(ANode::TyANY).c_str(),
	      ANode::TyANY)
};

//*****************************************************************************
// ANodeChildIterator
//*****************************************************************************

ANodeChildIterator::ANodeChildIterator(const ANode* root,
				       const ANodeFilter* f)
  : NonUniformDegreeTreeNodeChildIterator(root, /*firstToLast*/ false),
    filter(f)
{
}

NonUniformDegreeTreeNode* 
ANodeChildIterator::Current() const
{
  NonUniformDegreeTreeNode *s; 
  ANode *si; 
  while ( (s = NonUniformDegreeTreeNodeChildIterator::Current()) ) {
    si = dynamic_cast<ANode*>(s);  
    if ((filter == NULL) || filter->Apply(*si)) { 
      break; 	
    }
    ((ANodeChildIterator*) this)->operator++(); 
  } 
  return dynamic_cast<ANode*>(s); 
} 

//*****************************************************************************
// CSProfTreeIterator
//*****************************************************************************

ANodeIterator::ANodeIterator(const ANode* root, 
			     const ANodeFilter* f, 
			     bool leavesOnly, 
			     TraversalOrder torder)
  : NonUniformDegreeTreeIterator(root, torder, 
	             (leavesOnly) ? NON_UNIFORM_DEGREE_TREE_ENUM_LEAVES_ONLY
				 : NON_UNIFORM_DEGREE_TREE_ENUM_ALL_NODES),
    filter(f)
{
}


NonUniformDegreeTreeNode* 
ANodeIterator::Current() const
{
  NonUniformDegreeTreeNode *s; 
  ANode *si; 
  while ( (s = NonUniformDegreeTreeIterator::Current()) ) {
    si = dynamic_cast<ANode*>(s); 
    DIAG_Assert(si != NULL, ""); 
    if ((filter == NULL) || filter->Apply(*si)) { 
      break; 	
    }
    ((ANodeIterator*) this)->operator++(); 
  } 
  return dynamic_cast<ANode*>(s); 
} 

//*****************************************************************************
// ANodeSortedIterator
//*****************************************************************************

ANodeSortedIterator::
ANodeSortedIterator(const ANode* node,
		    ANodeSortedIterator::cmp_fptr_t compare_fn,
		    const ANodeFilter* filterFunc,
		    bool leavesOnly)
{
  ANodeIterator it(node, filterFunc, leavesOnly); 
  ANode *cur; 
  for (; (cur = it.CurNode()); ) {
    scopes.Add((unsigned long) cur); 
    it++; 
  }
  ptrSetIt = new WordSetSortedIterator(&scopes, compare_fn); 
}

ANodeSortedIterator::~ANodeSortedIterator() 
{
  delete ptrSetIt; 
}
 
ANode* 
ANodeSortedIterator::Current() const
{
  ANode *cur = NULL; 
  if (ptrSetIt->Current()) {
    cur = (ANode*) (*ptrSetIt->Current()); 
    DIAG_Assert(cur != NULL, ""); 
  }
  return cur; 
} 

void 
ANodeSortedIterator::DumpAndReset(ostream& os)
{
  os << "ANodeSortedIterator: " << endl; 
  while (Current()) {
    os << Current()->toString_me() << endl; 
    (*this)++; 
  } 
  Reset(); 
}

void 
ANodeSortedIterator::Reset()
{
  ptrSetIt->Reset(); 
}


int 
ANodeSortedIterator::cmpByName(const void* a, const void* b)
{
  ANode* x = (*(ANode**)a); 
  ANode* y = (*(ANode**)b); 
  DIAG_Assert (x != NULL, "");
  DIAG_Assert (y != NULL, "");
  return strcmp(x->name().c_str(), y->name().c_str()); 
}


int
ANodeSortedIterator::cmpByLine(const void* a, const void* b) 
{
  ANode* x = (*(ANode**)a); 
  ANode* y = (*(ANode**)b); 
  DIAG_Assert(x != NULL, "");
  DIAG_Assert(y != NULL, "");
  return ANodeLineComp(x, y);
}


int
ANodeSortedIterator::cmpByStructureId(const void* a, const void* b) 
{
  ANode* x = (*(ANode**)a);
  ANode* y = (*(ANode**)b);
  DIAG_Assert(x && y , "");
  uint x_id = x->structure() ? x->structure()->id() : 0;
  uint y_id = y->structure() ? y->structure()->id() : 0;
  return (x_id - y_id);
}

//*****************************************************************************
// ANodeSortedChildIterator
//*****************************************************************************

ANodeSortedChildIterator::
ANodeSortedChildIterator(const ANode* scope, 
			 ANodeSortedIterator::cmp_fptr_t compare_fn,
			 const ANodeFilter* f)
{
  ANodeChildIterator it(scope, f); 
  ANode* cur; 
  for (; (cur = it.CurNode()); ) {
    scopes.Add((unsigned long) cur); 
    it++; 
  }
  ptrSetIt = new WordSetSortedIterator(&scopes, compare_fn); 
}

ANodeSortedChildIterator::~ANodeSortedChildIterator() 
{
  delete ptrSetIt; 
}
 
ANode* 
ANodeSortedChildIterator::Current() const
{
  ANode *cur = NULL; 
  if (ptrSetIt->Current()) {
    cur = (ANode*) (*ptrSetIt->Current()); 
    DIAG_Assert(cur != NULL, "");
  }
  return cur; 
}

void 
ANodeSortedChildIterator::Reset()
{
  ptrSetIt->Reset(); 
}

void
ANodeSortedChildIterator::DumpAndReset(ostream& os)
{
  os << "ANodeSortedChildIterator: " << endl; 
  while (Current()) {
    os << Current()->toString_me() << endl; 
    (*this)++; 
  } 
  Reset(); 
}

//***************************************************************************

} // namespace CCT

} // namespace Prof
