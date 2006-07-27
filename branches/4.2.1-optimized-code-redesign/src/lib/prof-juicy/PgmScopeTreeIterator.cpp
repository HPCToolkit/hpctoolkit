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

//*************************** User Include Files ****************************

#include "PgmScopeTree.hpp"
#include "PgmScopeTreeIterator.hpp"
#include <lib/support/Assertion.h>

//*************************** Forward Declarations **************************

static int CompareByLine(const void *a, const void *b);

//***************************************************************************

//***************************************************************************
// ScopeInfoFilter support
//***************************************************************************

bool 
HasScopeType(const ScopeInfo& sinfo, long type)
{
  return (type == ScopeInfo::ANY
	  || sinfo.Type() == ScopeInfo::IntToScopeType(type));
}

const ScopeInfoFilter ScopeTypeFilter[ScopeInfo::NUMBER_OF_SCOPES] = {
  ScopeInfoFilter(HasScopeType,
		  ScopeInfo::ScopeTypeToName(ScopeInfo::PGM),
		  ScopeInfo::PGM),
  ScopeInfoFilter(HasScopeType,
		  ScopeInfo::ScopeTypeToName(ScopeInfo::GROUP),
		  ScopeInfo::GROUP),
  ScopeInfoFilter(HasScopeType,
		  ScopeInfo::ScopeTypeToName(ScopeInfo::LM), 
		  ScopeInfo::LM),
  ScopeInfoFilter(HasScopeType,
		  ScopeInfo::ScopeTypeToName(ScopeInfo::FILE),
		  ScopeInfo::FILE),
  ScopeInfoFilter(HasScopeType,
		  ScopeInfo::ScopeTypeToName(ScopeInfo::PROC),
		  ScopeInfo::PROC),
  ScopeInfoFilter(HasScopeType,
		  ScopeInfo::ScopeTypeToName(ScopeInfo::LOOP),
		  ScopeInfo::LOOP),
  ScopeInfoFilter(HasScopeType,
		  ScopeInfo::ScopeTypeToName(ScopeInfo::STMT_RANGE),
		  ScopeInfo::STMT_RANGE),
  ScopeInfoFilter(HasScopeType,
		  ScopeInfo::ScopeTypeToName(ScopeInfo::REF),
		  ScopeInfo::REF),
  ScopeInfoFilter(HasScopeType,
		  ScopeInfo::ScopeTypeToName(ScopeInfo::ANY),
		  ScopeInfo::ANY)
};

//***************************************************************************
// ScopeInfoChildIterator
//***************************************************************************

ScopeInfoChildIterator::ScopeInfoChildIterator(const ScopeInfo *root, 
					       const ScopeInfoFilter* f) 
  : NonUniformDegreeTreeNodeChildIterator(root, /*firstToLast*/ false),
    filter(f)
{ }

NonUniformDegreeTreeNode* 
ScopeInfoChildIterator::Current()  const
{
  NonUniformDegreeTreeNode *s;
  ScopeInfo *si;
  while ( (s = NonUniformDegreeTreeNodeChildIterator::Current()) ) {
    si = dynamic_cast<ScopeInfo*>(s);
    if ((filter == NULL) || filter->Apply(*si)) { 
      break; 	
    }
    ((ScopeInfoChildIterator*) this)->operator++();
  } 
  return dynamic_cast<ScopeInfo*>(s);
} 


//***************************************************************************
// CodeInfoChildIterator
//***************************************************************************

CodeInfoChildIterator::CodeInfoChildIterator(const CodeInfo *root) 
  : NonUniformDegreeTreeNodeChildIterator(root, /*firstToLast*/ false)
  {}

//***************************************************************************
// ScopeInfoIterator
//***************************************************************************

ScopeInfoIterator::ScopeInfoIterator(const ScopeInfo *root, 
	                             const ScopeInfoFilter *f, 
                                     bool leavesOnly, 
				     TraversalOrder torder)
  : NonUniformDegreeTreeIterator(root, torder, 
			(leavesOnly) ? NON_UNIFORM_DEGREE_TREE_ENUM_LEAVES_ONLY
				 : NON_UNIFORM_DEGREE_TREE_ENUM_ALL_NODES),
  filter(f)
{
}

NonUniformDegreeTreeNode* 
ScopeInfoIterator::Current()  const
{
  NonUniformDegreeTreeNode *s;
  ScopeInfo *si;
  while ( (s = NonUniformDegreeTreeIterator::Current()) ) {
    si = dynamic_cast<ScopeInfo*>(s);
    BriefAssertion(si != NULL);
    if ((filter == NULL) || filter->Apply(*si)) { 
      break; 	
    }
    ((ScopeInfoIterator*) this)->operator++();
  } 
  return dynamic_cast<ScopeInfo*>(s);
} 

//***************************************************************************
// ScopeInfoLineSortedIterator
//***************************************************************************

ScopeInfoLineSortedIterator::
ScopeInfoLineSortedIterator(const CodeInfo *file, 
			    const ScopeInfoFilter *filterFunc, 
			    bool leavesOnly)
{
  ScopeInfoIterator it(file, filterFunc, leavesOnly);
  ScopeInfo *cur;
  for (; (cur = it.CurScope()); ) {
    scopes.Add((unsigned long) cur);
    it++;
  }
  ptrSetIt = new WordSetSortedIterator(&scopes, CompareByLine);
}


ScopeInfoLineSortedIterator::~ScopeInfoLineSortedIterator() 
{
  delete ptrSetIt;
}
 
CodeInfo* 
ScopeInfoLineSortedIterator::Current() const
{
  CodeInfo *cur = NULL;
  if (ptrSetIt->Current()) {
    cur = (CodeInfo*) (*ptrSetIt->Current());
    BriefAssertion(cur != NULL);
  }
  return cur;
} 

void 
ScopeInfoLineSortedIterator::DumpAndReset(ostream &os)
{
  os << "ScopeInfoLineSortedIterator: " << endl;
  while (Current()) {
    os << Current()->ToString() << endl;
    (*this)++;
  } 
  Reset();
}

void 
ScopeInfoLineSortedIterator::Reset()
{
  ptrSetIt->Reset();
}

static int 
CompareByLine(const void* a, const void *b) 
{
  CodeInfo* x = (*(CodeInfo**)a);
  CodeInfo* y = (*(CodeInfo**)b);
  BriefAssertion (x != NULL);
  BriefAssertion (y != NULL);
  return CodeInfoLineComp(x, y);
}

//***************************************************************************
// ScopeInfoLineSortedChildIterator
//***************************************************************************

ScopeInfoLineSortedChildIterator::
ScopeInfoLineSortedChildIterator(const ScopeInfo *scope, 
				 const ScopeInfoFilter * f)
{
  ScopeInfoChildIterator it(scope, f);
  ScopeInfo *cur;
  for (; (cur = it.CurScope()); ) {
    scopes.Add((unsigned long) cur);
    it++;
  }
  ptrSetIt = new WordSetSortedIterator(&scopes, CompareByLine);
}

ScopeInfoLineSortedChildIterator::~ScopeInfoLineSortedChildIterator()
{
  delete ptrSetIt;
}

CodeInfo*
ScopeInfoLineSortedChildIterator::Current() const
{
  CodeInfo *cur = NULL;
  if (ptrSetIt->Current()) {
    cur = (CodeInfo*) (*ptrSetIt->Current());
    BriefAssertion(cur != NULL);
  }
  return cur;
}

void
ScopeInfoLineSortedChildIterator::Reset()
{
  ptrSetIt->Reset();
}

void
ScopeInfoLineSortedChildIterator::DumpAndReset(ostream &os)
{
  os << "ScopeInfoLineSortedChildIterator: " << endl;
  while (Current()) {
    os << Current()->ToString() << endl;
    (*this)++;
  }
  Reset();
}


//***************************************************************************
// ScopeInfoNameSortedChildIterator
//***************************************************************************

ScopeInfoNameSortedChildIterator::
ScopeInfoNameSortedChildIterator(const ScopeInfo *scope, 
				 const ScopeInfoFilter * f)
{
  ScopeInfoChildIterator it(scope, f);
  ScopeInfo *cur;
  for (; (cur = it.CurScope()); ) {
    scopes.Add((unsigned long) cur);
    it++;
  }
  ptrSetIt = new WordSetSortedIterator(&scopes, CompareByName);
}

ScopeInfoNameSortedChildIterator::~ScopeInfoNameSortedChildIterator() 
{
  delete ptrSetIt;
}
 
CodeInfo* 
ScopeInfoNameSortedChildIterator::Current() const
{
  CodeInfo *cur = NULL;
  if (ptrSetIt->Current()) {
    cur = (CodeInfo*) (*ptrSetIt->Current());
    BriefAssertion(cur != NULL);
  }
  return cur;
}

void 
ScopeInfoNameSortedChildIterator::Reset()
{
  ptrSetIt->Reset();
}

int 
ScopeInfoNameSortedChildIterator::CompareByName(const void* a, const void *b) 
{
  ScopeInfo* x = (*(ScopeInfo**)a);
  ScopeInfo* y = (*(ScopeInfo**)b);
  BriefAssertion (x != NULL);
  BriefAssertion (y != NULL);
  return strcmp(x->Name(), y->Name());
}

//***************************************************************************
