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

#ifndef PgmScopeTreeIterator_h
#define PgmScopeTreeIterator_h

//************************* System Include Files ****************************

#include <iostream>

//*************************** User Include Files ****************************

#include <include/general.h>

#include "PgmScopeTree.hpp"
#include <lib/support/NonUniformDegreeTree.hpp>
#include <lib/support/PtrSetIterator.hpp>

//*************************** Forward Declarations **************************

int HasPerfData( const ScopeInfo *si, int nm );
void UpdateScopeTree( const ScopeInfo *node, int noMetrics );

//***************************************************************************
// ScopeInfoFilter
//    filters are used in iterators to make sure only desirable ScopeInfos 
//    are iterated 
// ScopeTypeFilter is an array of globally defined filters that return true 
//    for ScopeInfos with a given type only 
// HasScopeType 
//    global fct used in filters from ScopeTypeFilter array
//***************************************************************************

typedef bool (*ScopeInfoFilterFct)(const ScopeInfo &info, long addArg);

class ScopeInfoFilter {
public:
   ScopeInfoFilter(ScopeInfoFilterFct f, const char* n, long a) 
     : fct(f), name(n), arg(a) { }

   virtual ~ScopeInfoFilter() { }
   bool Apply(const ScopeInfo& s) const { return fct(s, arg); }
   const char* Name() const { return name; }

private:
   const ScopeInfoFilterFct fct;
   const char* name;
   long arg;
};

// HasScopeType(s,tp) == ((tp == ANY) || (s.Type() == tp));
extern bool HasScopeType(const ScopeInfo &sinfo, long type);

// ScopeTypeFile[tp].Apply(s) == HasScopeType(s,tp) 
extern const ScopeInfoFilter ScopeTypeFilter[ScopeInfo::NUMBER_OF_SCOPES];


//***************************************************************************
// ScopeInfoChildIterator
//    iterates over all children of a ScopeInfo that pass the given 
//    filter or if a NULL filter was given over all children 
//    no particular order is garanteed 
//***************************************************************************

class ScopeInfoChildIterator : public NonUniformDegreeTreeNodeChildIterator {
public: 
  ScopeInfoChildIterator(const ScopeInfo *root, 
			 const ScopeInfoFilter *filter = NULL);
	             // filter == NULL enumerate all entries
	             // otherwise: only entries with filter->fct(e) == true
  
  virtual NonUniformDegreeTreeNode* Current() const; // really ScopeInfo
  
  ScopeInfo* CurScope() const { return dynamic_cast<ScopeInfo*>(Current()); }
private: 
  const ScopeInfoFilter *filter;
};


//***************************************************************************
// CodeInfoChildIterator
//    iterates over all children of a ScopeInfo that pass the given 
//    filter or if a NULL filter was given over all children 
//    no particular order is garanteed 
//***************************************************************************

class CodeInfoChildIterator : public NonUniformDegreeTreeNodeChildIterator {
public: 
  CodeInfoChildIterator(const CodeInfo *root);
  CodeInfo* CurCodeInfo() const { return dynamic_cast<CodeInfo*>(Current()); }
};  


//***************************************************************************
// ScopeInfoIterator
//    iterates over all ScopeInfos in the tree rooted at a given ScopeInfo 
//    that pass the given filter or if a NULL filter was given over all 
//    ScopeInfos in the tree.
//    no particular order is garanteed, unless stated explicitly , i.e. we 
//    may change the default value for torder
//***************************************************************************

class ScopeInfoIterator : public NonUniformDegreeTreeIterator {
public: 
   // filter == NULL enumerate all entries
   // otherwise: only entries with filter->fct(e) == true
   ScopeInfoIterator(const ScopeInfo *root,
		     const ScopeInfoFilter* filter = NULL,
		     bool leavesOnly = false,
		     TraversalOrder torder = PreOrder);
   
  virtual NonUniformDegreeTreeNode* Current() const; // really ScopeInfo
  
  ScopeInfo* CurScope() const { return dynamic_cast<ScopeInfo*>(Current()); }
private: 
  const ScopeInfoFilter *filter;
};  


//***************************************************************************
// ScopeInfoLineSortedIterator
//
// ScopeInfoLineSortedChildIterator
//    behaves as ScopeInfoIterator (PreOrder), 
//    except that it gurantees LineOrder among siblings  
//
// LineOrder: CodeInfo* a is enumerated before b 
//                    iff a->StartLine() < b->StartLine() 
//            RefInfo * a is enumerated before b 
//                    iff a->StartPos() < b->StartPos() 
//
// NOTE: the implementation was generalized so that it no longer assumes
//       that children in the tree contain non-overlapping ranges. all
//       lines are gathered into a set, sorted, and then enumerated out
//       of the set in sorted order. -- johnmc 5/31/00
//***************************************************************************

class ScopeInfoLineSortedIterator {
public: 
  ScopeInfoLineSortedIterator(const CodeInfo *file, 
			      const ScopeInfoFilter* filterFunc = NULL, 
			      bool leavesOnly = true);
  ~ScopeInfoLineSortedIterator();
  
  CodeInfo* Current() const;
  void  operator++(int)   { (*ptrSetIt)++;}
  void Reset();
  void DumpAndReset(std::ostream &os = std::cerr);

private:
  WordSet scopes;  // the scopes we want to have sorted
  WordSetSortedIterator *ptrSetIt;  
};

class ScopeInfoLineSortedChildIterator {
public: 
  ScopeInfoLineSortedChildIterator(const ScopeInfo *root, 
				   const ScopeInfoFilter* filterFunc = NULL);
  ~ScopeInfoLineSortedChildIterator();

  CodeInfo* Current() const;
  void  operator++(int)   { (*ptrSetIt)++;}
  void Reset();
  void DumpAndReset(std::ostream &os = std::cerr);

  // both of these are buried in other parts of the HPCView code :-(
  CodeInfo* CurScope() const { return dynamic_cast<CodeInfo*>(Current()); }
  CodeInfo* CurCode()  const { return dynamic_cast<CodeInfo*>(Current()); }

private:
  WordSet scopes;  // the scopes we want to have sorted
  WordSetSortedIterator* ptrSetIt;
};

//***************************************************************************
// CodeInfoLine
//  is a dummy class used just for the large scopes iterators.
//  the instance can be created either for the start line or for the end line
//
// FIXME: do we need this?
//***************************************************************************

#define IS_BEG_LINE 0
#define IS_END_LINE 1

class CodeInfoLine
{
public:
   CodeInfoLine( CodeInfo* ci, int forEndLine ) 
     : _ci(ci), _forEndLine(forEndLine), _type(ci->Type())
   {
   }
   
   int IsEndLine()    { return _forEndLine; }
   
   CodeInfo* GetCodeInfo()  { return _ci; }
   
   suint GetLine()
   { 
      return (_forEndLine ? _ci->endLine() : _ci->begLine());
   }
   
   ScopeInfo::ScopeType Type()   { return _type; }
   
private:
   CodeInfo* _ci;
   int _forEndLine;
   ScopeInfo::ScopeType _type;
};

//***************************************************************************
// ScopeInfoLineSortedIteratorForLargeScopes
//
//   behaves as ScopeInfoLineSortedIterator, but it consider both the
//   StartLine and the EndLine for scopes such as LOOPs or PROCs or
//   even GROUPs.
//    -- mgabi 08/18/01
//   it gurantees LineOrder among siblings  
//
// LineOrder: CodeInfo* a is enumerated before b 
//                    iff a->StartLine() < b->StartLine() 
//            RefInfo * a is enumerated before b 
//                    iff a->StartPos() < b->StartPos() 
//
// NOTE: the implementation was generalized so that it no longer assumes
//       that children in the tree contain non-overlapping ranges. all
//       lines are gathered into a set, sorted, and then enumerated out
//       of the set in sorted order. -- johnmc 5/31/00
//
// FIXME: do we need this?
//***************************************************************************

class ScopeInfoLineSortedIteratorForLargeScopes {
public: 
  ScopeInfoLineSortedIteratorForLargeScopes(const CodeInfo *file, 
			      const ScopeInfoFilter *filterFunc = NULL, 
			      bool leavesOnly = true);
  ~ScopeInfoLineSortedIteratorForLargeScopes();
  
  CodeInfoLine* Current() const;
  void  operator++(int)   { (*ptrSetIt)++;}
  void Reset();
  void DumpAndReset(std::ostream &os = std::cerr);

private:
  WordSet scopes;  // the scopes we want to have sorted
  WordSetSortedIterator *ptrSetIt;  
};

//***************************************************************************
// ScopeInfoNameSortedChildIterator
//    behaves as ScopeInfoChildIterator, except that it gurantees NameOrder 
//    NameOrder: a is enumerated before b iff a->Name() < b->Name() 
//***************************************************************************

class ScopeInfoNameSortedChildIterator {
public: 
  ScopeInfoNameSortedChildIterator(const ScopeInfo *root,
				   const ScopeInfoFilter* filterFunc = NULL);
  ~ScopeInfoNameSortedChildIterator();
  
  CodeInfo* Current() const;
  void  operator++(int)   { (*ptrSetIt)++;}
  void Reset();

private:
  static int CompareByName(const void *a, const void *b);
  WordSet scopes;  // the scopes we want to have sorted
  WordSetSortedIterator *ptrSetIt;  
};

//***************************************************************************
// SortedCodeInfoIterator
//    behaves as ScopeInfoIterator, except that it gurantees PerformanceOrder
//    PerformanceOrder:
//          a is enumerated before b
//          iff a->PerfInfos().GetVal(i) > b->PerfInfos().GetVal(i)
//             where GetVal(i) retrieves the value for the performace
//             statistic that the iterator is to sort for
//
// FIXME: do we need this
//***************************************************************************

class SortedCodeInfoIterator {
public:
  SortedCodeInfoIterator(const PgmScope *scope, int perfInfoIndex, 
			  const ScopeInfoFilter *filterFunc = NULL);
  virtual ~SortedCodeInfoIterator();

  CodeInfo *Current() const;
  void  operator++(int)   { (*ptrSetIt)++;}
  void Reset();
private:
  int compareByPerfInfo;

  WordSet scopes;  // the scopes we want to have sorted
  WordSetSortedIterator *ptrSetIt;
};


//***************************************************************************
// SortedCodeInfoChildIterator
//    behaves as ScopeInfoChildIterator, except that it guarantees children in 
//    PerformanceOrder.
//
//    PerformanceOrder:
//          a is enumerated before b
//          iff a->PerfInfos().GetVal(i) > b->PerfInfos().GetVal(i)
//             where GetVal(i) retrieves the value for the performace
//             statistic that the iterator is to sort for
//
// FIXME: do we need this
//***************************************************************************

class SortedCodeInfoChildIterator {
public:
  SortedCodeInfoChildIterator(const ScopeInfo *scope, int flattenDepth,
			      int compare(const void* a, const void *b),
			      const ScopeInfoFilter *filterFunc = NULL);
  virtual ~SortedCodeInfoChildIterator();

  CodeInfo *Current() const;
  void  operator++(int)   { (*ptrSetIt)++;}
  void Reset();
private:
  void AddChildren(const ScopeInfo *scope, int curDepth,
		   const ScopeInfoFilter *filterFunc);
private:
  WordSet scopes;  // the scopes we want to have sorted
  WordSetSortedIterator *ptrSetIt;
  int depth;
};

//***************************************************************************
// Function CompareByPerfInfo
//
// Description:
//   helper function used by sorting iterators to sort by a particular
//   performance metric.
// 
//   CompareByPerfInfo compares by the performance metric whose index is 
//   specified by the global variable  
// 
//   All sorting iterators use the comparison function only in the 
//   constructor, so the value of CompareByPerfInfo_MetricIndex needs only 
//   to remain constant for the duration of the constructor to which 
//   CompareByPerfInfo is passed.
//
// FIXME: do we need this
//***************************************************************************

int CompareByPerfInfo(const void* a, const void *b);
extern int CompareByPerfInfo_MetricIndex;

#endif 
