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
//    PgmScopeTreeIterator.h
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

#include "PgmScopeTree.h"
#include <lib/support/NonUniformDegreeTree.h>  
#include <lib/support/PtrSetIterator.h>        

//*************************** Forward Declarations ***************************

class ScopesInfo; 

//*****************************************************************************
// ScopeInfoFilter
//    filters are used in iterators to make sure only desirable ScopeInfos 
//    are iterated 
// ScopeTypeFilter is an array of globally defined filters that return true 
//    for ScopeInfos with a given type only 
// HasScopeType 
//    global fct used in filters from ScopeTypeFilter array
//*****************************************************************************

typedef bool (*ScopeInfoFilterFct)(const ScopeInfo &info, long addArg);

class ScopeInfoFilter {
public:
   ScopeInfoFilter(ScopeInfoFilterFct f, const char* n, long a)
     : fct(f), arg(a), name(n) {};

   virtual ~ScopeInfoFilter() { };
   bool Apply(const ScopeInfo& s) const { return fct(s, arg); }
   const char* Name() const {return name;};
private:
   const ScopeInfoFilterFct fct; 
   long arg; 
   const char* name; 
};

// HasScopeType(s,tp) == ((tp == ANY) || (s.Type() == tp));
extern bool HasScopeType(const ScopeInfo &sinfo, long type); 

// ScopeTypeFile[tp].Apply(s) == HasScopeType(s,tp) 
extern const ScopeInfoFilter ScopeTypeFilter[ScopeInfo::NUMBER_OF_SCOPES];


//*****************************************************************************
// ScopeInfoChildIterator
//    iterates over all children of a ScopeInfo that pass the given 
//    filter or if a NULL filter was given over all children 
//    no particular order is garanteed 
//*****************************************************************************

class ScopeInfoChildIterator : public NonUniformDegreeTreeNodeChildIterator {
public: 
  ScopeInfoChildIterator(const ScopeInfo *root, 
			 const ScopeInfoFilter *filter = NULL); 
	             // filter == NULL enumerate all entries
	             // otherwise: only entries with filter->fct(e) == true
  
  virtual NonUniformDegreeTreeNode* Current() const; // really ScopeInfo
  
  ScopeInfo* CurScope() const { return dynamic_cast<ScopeInfo*>(Current()); };
private: 
   const ScopeInfoFilter *filter; 
};  


//*****************************************************************************
// CodeInfoChildIterator
//    iterates over all children of a ScopeInfo that pass the given 
//    filter or if a NULL filter was given over all children 
//    no particular order is garanteed 
//*****************************************************************************

class CodeInfoChildIterator : public NonUniformDegreeTreeNodeChildIterator {
public: 
  CodeInfoChildIterator(const CodeInfo *root); 
  CodeInfo* CurCodeInfo() const { return dynamic_cast<CodeInfo*>(Current()); };
};  


//*****************************************************************************
// ScopeInfoIterator
//    iterates over all ScopeInfos in the tree rooted at a given ScopeInfo 
//    that pass the given filter or if a NULL filter was given over all 
//    ScopeInfos in the tree.
//    no particular order is garanteed, unless stated explicitly , i.e. we 
//    may change the default value for torder
//*****************************************************************************

class ScopeInfoIterator : public NonUniformDegreeTreeIterator {
public: 
   // filter == NULL enumerate all entries
   // otherwise: only entries with filter->fct(e) == true
   ScopeInfoIterator(const ScopeInfo *root, 
		     const ScopeInfoFilter *filter = NULL, 
		     bool leavesOnly = false, 
		     TraversalOrder torder = PreOrder); 
   
  virtual NonUniformDegreeTreeNode* Current() const; // really ScopeInfo
  
  ScopeInfo* CurScope() const { return dynamic_cast<ScopeInfo*>(Current()); }; 
private: 
  const ScopeInfoFilter *filter; 
};  


//*****************************************************************************
// ScopeInfoLineSortedIterator
//
// ScopeInfoLineSortedChildIterator
//    behaves as ScopeInfoIterator (PreOrder), 
//    except that it gurantees LineOrder among siblings  
//
// LineOrder: CodeInfo* a is enumerated before b 
//                    iff a->StartLine() < b->StartLine() 
//
// NOTE: the implementation was generalized so that it no longer assumes
//       that children in the tree contain non-overlapping ranges. all
//       lines are gathered into a set, sorted, and then enumerated out
//       of the set in sorted order. -- johnmc 5/31/00
//*****************************************************************************

class ScopeInfoLineSortedIterator {
public: 
  ScopeInfoLineSortedIterator(const CodeInfo *file, 
			      const ScopeInfoFilter *filterFunc = NULL, 
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
				   const ScopeInfoFilter *filterFunc = NULL);
  ~ScopeInfoLineSortedChildIterator(); 
  
  CodeInfo* Current() const; 
  void  operator++(int)   { (*ptrSetIt)++;}
  void Reset();
  void DumpAndReset(std::ostream &os = std::cerr);

private:
  WordSet scopes;  // the scopes we want to have sorted
  WordSetSortedIterator *ptrSetIt;  
};

//*****************************************************************************
// ScopeInfoNameSortedChildIterator
//    behaves as ScopeInfoChildIterator, except that it gurantees NameOrder 
//    NameOrder: a is enumerated before b iff a->Name() < b->Name() 
//*****************************************************************************

class ScopeInfoNameSortedChildIterator {
public: 
  ScopeInfoNameSortedChildIterator(const ScopeInfo *root, 
				   const ScopeInfoFilter *filterFunc = NULL);
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

#endif 
