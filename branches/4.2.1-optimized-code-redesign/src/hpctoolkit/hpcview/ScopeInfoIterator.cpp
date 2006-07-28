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

#include <vector>

//*************************** User Include Files ****************************

#include "ScopeInfo.hpp"
#include "ScopeInfoIterator.hpp"
#include "PerfMetric.hpp"
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
// HasPerfData
//    check if this node has any profile data associated with it
//***************************************************************************

int 
HasPerfData( const ScopeInfo *si, int nm )
{
  for ( int i = 0 ; i<nm ; i++ )
    if (si->HasPerfData(i))
      return 1;
  return 0;
}


//***************************************************************************
// UpdateScopeTree - not a member of any class
//    traverses a tree and removes all the nodes that don't have any profile
//    data associated
//***************************************************************************

void 
UpdateScopeTree( const ScopeInfo *node, int noMetrics )
{
  ScopeInfo *si;
  std::vector<ScopeInfo*> toBeRemoved;
  
  ScopeInfoChildIterator it( node, NULL );
  for ( ; it.Current() ; it++ ) {
    si = dynamic_cast<ScopeInfo*>(it.Current());
    if ( HasPerfData( si, noMetrics ) )
      UpdateScopeTree( si, noMetrics );
    else
      toBeRemoved.push_back(si);
  }
  for ( unsigned int i=0 ; i<toBeRemoved.size() ; i++ ) {
    si = toBeRemoved[i];
    si->Unlink();
    switch( si->Type() )
      {
      case ScopeInfo::PGM: delete dynamic_cast<PgmScope*>(si); break;
      case ScopeInfo::GROUP: delete dynamic_cast<GroupScope*>(si); break;
      case ScopeInfo::LM: delete dynamic_cast<LoadModScope*>(si); break;
      case ScopeInfo::FILE: delete dynamic_cast<FileScope*>(si); break;
      case ScopeInfo::PROC: delete dynamic_cast<ProcScope*>(si); break;
      case ScopeInfo::LOOP: delete dynamic_cast<LoopScope*>(si); break;
      case ScopeInfo::STMT_RANGE: delete dynamic_cast<StmtRangeScope*>(si); break;
      default: delete si; break;
      }
  }
}


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
// ScopeInfoLineSortedIteratorForLargeScopes
//***************************************************************************

ScopeInfoLineSortedIteratorForLargeScopes::ScopeInfoLineSortedIteratorForLargeScopes(
					const CodeInfo *file, 
					const ScopeInfoFilter *filterFunc, 
					bool leavesOnly)
{
  ScopeInfoIterator it(file, filterFunc, leavesOnly);
  ScopeInfo *cur;
  for (; (cur = it.CurScope()); ) {
    CodeInfoLine *cur1 = new CodeInfoLine(dynamic_cast<CodeInfo*>(cur), 
                         IS_BEG_LINE);
    scopes.Add((unsigned long) cur1);
    if (cur->Type() == ScopeInfo::LOOP)
    {  // create a CodeInfoLine object for both begin and end line
       // only for Loops. For PROCs we are interested only in BegLine
      CodeInfoLine *cur2 = new CodeInfoLine(dynamic_cast<CodeInfo*>(cur), 
                         IS_END_LINE);
      scopes.Add((unsigned long) cur2);
    }
    it++;
  }
  ptrSetIt = new WordSetSortedIterator(&scopes, CompareByLine);
}


ScopeInfoLineSortedIteratorForLargeScopes::~ScopeInfoLineSortedIteratorForLargeScopes() 
{
  // First deallocate all CodeInfoLine objects created
  ptrSetIt->Reset();
  for ( ; ptrSetIt->Current() ; (*ptrSetIt)++ )
    delete ((CodeInfoLine*) (*ptrSetIt->Current()));
  delete ptrSetIt;
}
 
CodeInfoLine* 
ScopeInfoLineSortedIteratorForLargeScopes::Current() const
{
  CodeInfoLine *cur = NULL;
  if (ptrSetIt->Current()) {
    cur = (CodeInfoLine*) (*ptrSetIt->Current());
    BriefAssertion(cur != NULL);
  }
  return cur;
} 

void 
ScopeInfoLineSortedIteratorForLargeScopes::DumpAndReset(std::ostream &os)
{
  os << "ScopeInfoLineSortedIteratorForLargeScopes: " << endl;
  while (Current()) {
    os << Current()->GetCodeInfo()->ToString() << endl;
    (*this)++;
  } 
  Reset();
}

void 
ScopeInfoLineSortedIteratorForLargeScopes::Reset()
{
  ptrSetIt->Reset();
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
// SortedCodeInfoIterator
//***************************************************************************

SortedCodeInfoIterator::SortedCodeInfoIterator(const PgmScope *pgm,
					       int perfInfoIndex,
					     const ScopeInfoFilter *filterFunc)
{
  BriefAssertion(pgm != NULL);
  compareByPerfInfo = perfInfoIndex;
  
  ScopeInfoIterator it(pgm, filterFunc, false);
  ScopeInfo *cur;
  for (; (cur = it.CurScope()); ) {
    scopes.Add((unsigned long) cur);
    it++;
  }
  CompareByPerfInfo_MetricIndex = compareByPerfInfo;
  ptrSetIt = new WordSetSortedIterator(&scopes, CompareByPerfInfo);
}

SortedCodeInfoIterator::~SortedCodeInfoIterator()
{
  delete ptrSetIt;
}

CodeInfo*
SortedCodeInfoIterator::Current() const
{
  CodeInfo *cur = NULL;
  if (ptrSetIt->Current()) {
    cur = (CodeInfo*) (*ptrSetIt->Current());
    BriefAssertion(cur != NULL);
  }
  return cur;
}

void
SortedCodeInfoIterator::Reset()
{
  ptrSetIt->Reset();
}


//***************************************************************************
// SortedCodeInfoChildIterator
//***************************************************************************

SortedCodeInfoChildIterator::SortedCodeInfoChildIterator
(const ScopeInfo *scope, int flattenDepth, 
 int compare(const void* a, const void *b),
 const ScopeInfoFilter *filterFunc)
{
  depth = flattenDepth;
  AddChildren(scope, 0, filterFunc);
  ptrSetIt = new WordSetSortedIterator(&scopes, compare);
}

SortedCodeInfoChildIterator::~SortedCodeInfoChildIterator()
{
  delete ptrSetIt;
}

CodeInfo*
SortedCodeInfoChildIterator::Current() const
{
  CodeInfo *cur = NULL;
  if (ptrSetIt->Current()) {
    cur = (CodeInfo*) (*ptrSetIt->Current());
    BriefAssertion(cur != NULL);
  }
  return cur;
}

void
SortedCodeInfoChildIterator::Reset()
{
  ptrSetIt->Reset();
}

void SortedCodeInfoChildIterator::AddChildren
(const ScopeInfo *scope, int curDepth, const ScopeInfoFilter *filterFunc)
{
  BriefAssertion(scope != NULL);
  
  ScopeInfoChildIterator it(scope, filterFunc);
  ScopeInfo *cur;
  for (; (cur = it.CurScope()); ) {
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

int CompareByPerfInfo_MetricIndex = -1;

int CompareByPerfInfo(const void* a, const void *b)
{
  ScopeInfo* x = *(ScopeInfo**) a;
  ScopeInfo* y = *(ScopeInfo**) b;
  BriefAssertion(x != NULL);
  BriefAssertion(y != NULL);
  double vx = 0.0;
  if (x->HasPerfData(CompareByPerfInfo_MetricIndex)) {
    vx = x->PerfData(CompareByPerfInfo_MetricIndex);
  }
  double vy = 0.0;
  if (y->HasPerfData(CompareByPerfInfo_MetricIndex)) {
    vy = y->PerfData(CompareByPerfInfo_MetricIndex);
  }
  double difference = vy-vx;
  
  if ( difference < 0) return -1;	
  else if ( difference > 0) return 1;	
  return 0;
}

