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

//*************************** User Include Files ****************************

#include "CCT-Tree.hpp"

#include <lib/support/diagnostics.h>

//*************************** Forward Declarations ***************************

using std::ostream;
using std::endl;

//****************************************************************************

namespace Prof {

static int CompareByLine(const void *a, const void *b);


//*****************************************************************************
// CSProfNodeFilter support
//*****************************************************************************

bool HasNodeType(const CSProfNode& sinfo, long type)
{
  return (type == CSProfNode::ANY) 
    || (sinfo.GetType() == CSProfNode::IntToNodeType(type)); 
}


const CSProfNodeFilter NodeTypeFilter[CSProfNode::NUMBER_OF_TYPES] = {
  CSProfNodeFilter(HasNodeType,
		   CSProfNode::NodeTypeToName(CSProfNode::PGM).c_str(),
		   CSProfNode::PGM),
  CSProfNodeFilter(HasNodeType,
		   CSProfNode::NodeTypeToName(CSProfNode::CALLSITE).c_str(),
		   CSProfNode::CALLSITE),
  CSProfNodeFilter(HasNodeType,
		   CSProfNode::NodeTypeToName(CSProfNode::LOOP).c_str(),
		   CSProfNode::LOOP),
  CSProfNodeFilter(HasNodeType,
		   CSProfNode::NodeTypeToName(CSProfNode::STMT_RANGE).c_str(),
		   CSProfNode::STMT_RANGE),
  CSProfNodeFilter(HasNodeType,
		   CSProfNode::NodeTypeToName(CSProfNode::PROCEDURE_FRAME).c_str(),
		   CSProfNode::PROCEDURE_FRAME),
  CSProfNodeFilter(HasNodeType,
		   CSProfNode::NodeTypeToName(CSProfNode::STATEMENT).c_str(),
		   CSProfNode::STATEMENT),
  CSProfNodeFilter(HasNodeType,
		   CSProfNode::NodeTypeToName(CSProfNode::ANY).c_str(),
		   CSProfNode::ANY)
};

//*****************************************************************************
// CSProfNodeChildIterator
//*****************************************************************************

CSProfNodeChildIterator::CSProfNodeChildIterator(const CSProfNode* root,
						 const CSProfNodeFilter* f)
  : NonUniformDegreeTreeNodeChildIterator(root, /*firstToLast*/ false),
    filter(f)
{
}

NonUniformDegreeTreeNode* 
CSProfNodeChildIterator::Current() const
{
  NonUniformDegreeTreeNode *s; 
  CSProfNode *si; 
  while ( (s = NonUniformDegreeTreeNodeChildIterator::Current()) ) {
    si = dynamic_cast<CSProfNode*>(s);  
    if ((filter == NULL) || filter->Apply(*si)) { 
      break; 	
    }
    ((CSProfNodeChildIterator*) this)->operator++(); 
  } 
  return dynamic_cast<CSProfNode*>(s); 
} 

//*****************************************************************************
// CSProfTreeIterator
//*****************************************************************************

CSProfNodeIterator::CSProfNodeIterator(const CSProfNode* root, 
				       const CSProfNodeFilter* f, 
				       bool leavesOnly, 
				       TraversalOrder torder)
  : NonUniformDegreeTreeIterator(root, torder, 
	             (leavesOnly) ? NON_UNIFORM_DEGREE_TREE_ENUM_LEAVES_ONLY
				 : NON_UNIFORM_DEGREE_TREE_ENUM_ALL_NODES),
  filter(f)

{
}

NonUniformDegreeTreeNode* 
CSProfNodeIterator::Current() const
{
  NonUniformDegreeTreeNode *s; 
  CSProfNode *si; 
  while ( (s = NonUniformDegreeTreeIterator::Current()) ) {
    si = dynamic_cast<CSProfNode*>(s); 
    DIAG_Assert(si != NULL, ""); 
    if ((filter == NULL) || filter->Apply(*si)) { 
      break; 	
    }
    ((CSProfNodeIterator*) this)->operator++(); 
  } 
  return dynamic_cast<CSProfNode*>(s); 
} 

//*****************************************************************************
// CSProfNodeSortedIterator
//*****************************************************************************

CSProfNodeSortedIterator::
CSProfNodeSortedIterator(const CSProfCodeNode* file,
			 CSProfNodeSortedIterator::cmp_fptr_t compare_fn,
			 const CSProfNodeFilter* filterFunc,
			 bool leavesOnly)
{
  CSProfNodeIterator it(file, filterFunc, leavesOnly); 
  CSProfNode *cur; 
  for (; (cur = it.CurNode()); ) {
    scopes.Add((unsigned long) cur); 
    it++; 
  }
  ptrSetIt = new WordSetSortedIterator(&scopes, compare_fn); 
}

CSProfNodeSortedIterator::~CSProfNodeSortedIterator() 
{
  delete ptrSetIt; 
}
 
CSProfCodeNode* 
CSProfNodeSortedIterator::Current() const
{
  CSProfCodeNode *cur = NULL; 
  if (ptrSetIt->Current()) {
    cur = (CSProfCodeNode*) (*ptrSetIt->Current()); 
    DIAG_Assert(cur != NULL, ""); 
  }
  return cur; 
} 

void 
CSProfNodeSortedIterator::DumpAndReset(ostream& os)
{
  os << "CSProfNodeSortedIterator: " << endl; 
  while (Current()) {
    os << Current()->toString_me() << endl; 
    (*this)++; 
  } 
  Reset(); 
}

void 
CSProfNodeSortedIterator::Reset()
{
  ptrSetIt->Reset(); 
}


int 
CSProfNodeSortedIterator::cmpByName(const void* a, const void* b)
{
  CSProfNode* x = (*(CSProfNode**)a); 
  CSProfNode* y = (*(CSProfNode**)b); 
  DIAG_Assert (x != NULL, "");
  DIAG_Assert (y != NULL, "");
  return strcmp(x->GetName().c_str(), y->GetName().c_str()); 
}


int
CSProfNodeSortedIterator::cmpByLine(const void* a, const void* b) 
{
  CSProfCodeNode* x = (*(CSProfCodeNode**)a); 
  CSProfCodeNode* y = (*(CSProfCodeNode**)b); 
  DIAG_Assert(x != NULL, "");
  DIAG_Assert(y != NULL, "");
  return CSProfCodeNodeLineComp(x, y);
}


int
CSProfNodeSortedIterator::cmpByStructureId(const void* a, const void* b) 
{
  CSProfCodeNode* x = (*(CSProfCodeNode**)a);
  CSProfCodeNode* y = (*(CSProfCodeNode**)b);
  DIAG_Assert(x && y , "");
  uint x_id = x->structure() ? x->structure()->id() : 0;
  uint y_id = y->structure() ? y->structure()->id() : 0;
  return (x_id - y_id);
}

//*****************************************************************************
// CSProfNodeSortedChildIterator
//*****************************************************************************

CSProfNodeSortedChildIterator::
CSProfNodeSortedChildIterator(const CSProfNode* scope, 
			      CSProfNodeSortedIterator::cmp_fptr_t compare_fn,
			      const CSProfNodeFilter* f)
{
  CSProfNodeChildIterator it(scope, f); 
  CSProfNode *cur; 
  for (; (cur = it.CurNode()); ) {
    scopes.Add((unsigned long) cur); 
    it++; 
  }
  ptrSetIt = new WordSetSortedIterator(&scopes, compare_fn); 
}

CSProfNodeSortedChildIterator::~CSProfNodeSortedChildIterator() 
{
  delete ptrSetIt; 
}
 
CSProfCodeNode* 
CSProfNodeSortedChildIterator::Current() const
{
  CSProfCodeNode *cur = NULL; 
  if (ptrSetIt->Current()) {
    cur = (CSProfCodeNode*) (*ptrSetIt->Current()); 
    DIAG_Assert(cur != NULL, "");
  }
  return cur; 
}

void 
CSProfNodeSortedChildIterator::Reset()
{
  ptrSetIt->Reset(); 
}

void
CSProfNodeSortedChildIterator::DumpAndReset(ostream& os)
{
  os << "CSProfNodeSortedChildIterator: " << endl; 
  while (Current()) {
    os << Current()->toString_me() << endl; 
    (*this)++; 
  } 
  Reset(); 
}

//***************************************************************************

} // namespace Prof
