// $Id$
// -*-C++-*-
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

//***************************************************************************
//
// File:
//    CSProfTreeIterator.C
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

//************************* System Include Files ****************************

//*************************** User Include Files ****************************

#include "CSProfTreeIterator.h"
#include <lib/support/Assertion.h>

//*************************** Forward Declarations ***************************

static int CompareByLine(const void *a, const void *b);

using std::ostream;
using std::endl;

//****************************************************************************

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
		   CSProfNode::NodeTypeToName(CSProfNode::PGM), 
		   CSProfNode::PGM),
  CSProfNodeFilter(HasNodeType,
		   CSProfNode::NodeTypeToName(CSProfNode::GROUP), 
		   CSProfNode::GROUP),
  CSProfNodeFilter(HasNodeType,
		   CSProfNode::NodeTypeToName(CSProfNode::GROUP), 
		   CSProfNode::CALLSITE),
  CSProfNodeFilter(HasNodeType,
		   CSProfNode::NodeTypeToName(CSProfNode::LOOP), 
		   CSProfNode::LOOP),
  CSProfNodeFilter(HasNodeType,
		   CSProfNode::NodeTypeToName(CSProfNode::STMT_RANGE),
		   CSProfNode::STMT_RANGE),
  CSProfNodeFilter(HasNodeType,
		   CSProfNode::NodeTypeToName(CSProfNode::ANY),
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
    BriefAssertion(si != NULL); 
    if ((filter == NULL) || filter->Apply(*si)) { 
      break; 	
    }
    ((CSProfNodeIterator*) this)->operator++(); 
  } 
  return dynamic_cast<CSProfNode*>(s); 
} 

//*****************************************************************************
// CSProfNodeLineSortedIterator
//*****************************************************************************

CSProfNodeLineSortedIterator::
CSProfNodeLineSortedIterator(const CSProfCodeNode* file,
			     const CSProfNodeFilter* filterFunc,
			     bool leavesOnly)
{
  CSProfNodeIterator it(file, filterFunc, leavesOnly); 
  CSProfNode *cur; 
  for (; (cur = it.CurNode()); ) {
    scopes.Add((unsigned long) cur); 
    it++; 
  }
  ptrSetIt = new WordSetSortedIterator(&scopes, CompareByLine); 
}

CSProfNodeLineSortedIterator::~CSProfNodeLineSortedIterator() 
{
  delete ptrSetIt; 
}
 
CSProfCodeNode* 
CSProfNodeLineSortedIterator::Current() const
{
  CSProfCodeNode *cur = NULL; 
  if (ptrSetIt->Current()) {
    cur = (CSProfCodeNode*) (*ptrSetIt->Current()); 
    BriefAssertion(cur != NULL); 
  }
  return cur; 
} 

void 
CSProfNodeLineSortedIterator::DumpAndReset(ostream& os)
{
  os << "CSProfNodeLineSortedIterator: " << endl; 
  while (Current()) {
    os << Current()->ToDumpString() << endl; 
    (*this)++; 
  } 
  Reset(); 
}

void 
CSProfNodeLineSortedIterator::Reset()
{
  ptrSetIt->Reset(); 
}

static int
CompareByLine(const void* a, const void* b) 
{
  CSProfCodeNode* x = (*(CSProfCodeNode**)a); 
  CSProfCodeNode* y = (*(CSProfCodeNode**)b); 
  BriefAssertion (x != NULL); 
  BriefAssertion (y != NULL);
  return CSProfCodeNodeLineComp(x, y);
}

//*****************************************************************************
// CSProfNodeLineSortedChildIterator
//*****************************************************************************

CSProfNodeLineSortedChildIterator::
CSProfNodeLineSortedChildIterator(const CSProfNode *scope, 
				  const CSProfNodeFilter * f)
{
  CSProfNodeChildIterator it(scope, f); 
  CSProfNode *cur; 
  for (; (cur = it.CurNode()); ) {
    scopes.Add((unsigned long) cur); 
    it++; 
  }
  ptrSetIt = new WordSetSortedIterator(&scopes, CompareByLine); 
}

CSProfNodeLineSortedChildIterator::~CSProfNodeLineSortedChildIterator() 
{
  delete ptrSetIt; 
}
 
CSProfCodeNode* 
CSProfNodeLineSortedChildIterator::Current() const
{
  CSProfCodeNode *cur = NULL; 
  if (ptrSetIt->Current()) {
    cur = (CSProfCodeNode*) (*ptrSetIt->Current()); 
    BriefAssertion(cur != NULL); 
  }
  return cur; 
}

void 
CSProfNodeLineSortedChildIterator::Reset()
{
  ptrSetIt->Reset(); 
}

void
CSProfNodeLineSortedChildIterator::DumpAndReset(ostream& os)
{
  os << "CSProfNodeLineSortedChildIterator: " << endl; 
  while (Current()) {
    os << Current()->ToDumpString() << endl; 
    (*this)++; 
  } 
  Reset(); 
}

//*****************************************************************************
// CSProfNodeNameSortedChildIterator
//*****************************************************************************

CSProfNodeNameSortedChildIterator::
CSProfNodeNameSortedChildIterator(const CSProfNode *scope, 
				  const CSProfNodeFilter * f)
{
  CSProfNodeChildIterator it(scope, f); 
  CSProfNode *cur; 
  for (; (cur = it.CurNode()); ) {
    scopes.Add((unsigned long) cur); 
    it++; 
  }
  ptrSetIt = new WordSetSortedIterator(&scopes, CompareByName); 
}

CSProfNodeNameSortedChildIterator::~CSProfNodeNameSortedChildIterator() 
{
  delete ptrSetIt; 
}
 
CSProfCodeNode* 
CSProfNodeNameSortedChildIterator::Current() const
{
  CSProfCodeNode *cur = NULL; 
  if (ptrSetIt->Current()) {
    cur = (CSProfCodeNode*) (*ptrSetIt->Current()); 
    BriefAssertion(cur != NULL); 
  }
  return cur; 
}

void 
CSProfNodeNameSortedChildIterator::Reset()
{
  ptrSetIt->Reset(); 
}

int 
CSProfNodeNameSortedChildIterator::CompareByName(const void* a, const void* b)
{
  CSProfNode* x = (*(CSProfNode**)a); 
  CSProfNode* y = (*(CSProfNode**)b); 
  BriefAssertion (x != NULL); 
  BriefAssertion (y != NULL); 
  return strcmp(x->GetName(), y->GetName()); 
}

//***************************************************************************
