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
//    $Source$
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef prof_juicy_Prof_Struct_TreeIterator_hpp
#define prof_juicy_Prof_Struct_TreeIterator_hpp

//************************* System Include Files ****************************

#include <iostream>

//*************************** User Include Files ****************************

#include <include/general.h>

//NOTE: included by PgmScopeTree.hpp
//#include "PgmScopeTree.hpp"

#include <lib/support/NonUniformDegreeTree.hpp>
#include <lib/support/PtrSetIterator.hpp>
#include <lib/support/SrcFile.hpp>


//*************************** Forward Declarations **************************

// tallent: For performance tuning; see usage below.  Unnecessary
// dynamic casting -- every NonUniformDegreeTreeNode is a ANode --
// was consuming 15-20% of execution time.
#define PGM_SCOPE_TREE__USE_DYNAMIC_CAST 0
#if (PGM_SCOPE_TREE__USE_DYNAMIC_CAST)
# define PST_USELESS_DCAST(t, x) dynamic_cast<t>(x)
#else
# define PST_USELESS_DCAST(t, x) (t)(x)
#endif


//***************************************************************************

namespace Prof {
namespace Struct {

//***************************************************************************
// ANodeFilter
//    filters are used in iterators to make sure only desirable ANodes 
//    are iterated 
// ANodeTyFilter is an array of globally defined filters that return true 
//    for ANodes with a given type only 
// HasANodeTy 
//    global fct used in filters from ANodeTyFilter array
//***************************************************************************

typedef bool (*ANodeFilterFct)(const ANode &info, long addArg);

class ANodeFilter {
public:
   ANodeFilter(ANodeFilterFct f, const char* n, long a) 
     : fct(f), name(n), arg(a) { }

   virtual ~ANodeFilter() { }
   bool Apply(const ANode& s) const { return fct(s, arg); }
   const char* Name() const { return name; }

private:
   const ANodeFilterFct fct;
   const char* name;
   long arg;
};

// HasANodeTy(s,tp) == ((tp == ANY) || (s.Type() == tp));
extern bool HasANodeTy(const ANode &sinfo, long type);

// ANodeTyFile[tp].Apply(s) == HasANodeTy(s,tp) 
extern const ANodeFilter ANodeTyFilter[ANode::TyNUMBER];


//***************************************************************************
// ANodeChildIterator
//    iterates over all children of a ANode that pass the given 
//    filter or if a NULL filter was given over all children 
//    no particular order is garanteed 
//***************************************************************************

class ANodeChildIterator : public NonUniformDegreeTreeNodeChildIterator {
public: 

  // filter == NULL enumerate all entries
  // otherwise: only entries with filter->fct(e) == true
  ANodeChildIterator(const ANode* root, 
			 const ANodeFilter* filter = NULL)
    : NonUniformDegreeTreeNodeChildIterator(root, /*firstToLast*/ false),
      filter(filter)
  { }

  
  // NOTE: really ANode
  virtual NonUniformDegreeTreeNode* 
  Current() const 
  {
    NonUniformDegreeTreeNode* s;
    ANode* si = NULL;
    while ( (s = NonUniformDegreeTreeNodeChildIterator::Current()) ) {
      si = PST_USELESS_DCAST(ANode*, s);
      if ((filter == NULL) || filter->Apply(*si)) { 
	break; 	
      }
      ((ANodeChildIterator*)this)->operator++();
    } 
    return PST_USELESS_DCAST(ANode*, s);
  }
  
  
  ANode* 
  CurScope() const 
  {
    return PST_USELESS_DCAST(ANode*, Current());
  }

private: 
  const ANodeFilter *filter;
};


//***************************************************************************
// ACodeNodeChildIterator
//    iterates over all children of a ANode that pass the given 
//    filter or if a NULL filter was given over all children 
//    no particular order is garanteed 
//***************************************************************************

class ACodeNodeChildIterator : public NonUniformDegreeTreeNodeChildIterator {
public: 
  ACodeNodeChildIterator(const ACodeNode *root)
    : NonUniformDegreeTreeNodeChildIterator(root, /*firstToLast*/ false)
  { }

  ACodeNode* 
  CurACodeNode() const 
  { 
    return dynamic_cast<ACodeNode*>(Current()); 
  }

};  


//***************************************************************************
// ANodeIterator
//    iterates over all ANodes in the tree rooted at a given ANode 
//    that pass the given filter or if a NULL filter was given over all 
//    ANodes in the tree.
//    no particular order is garanteed, unless stated explicitly , i.e. we 
//    may change the default value for torder
//***************************************************************************

class ANodeIterator : public NonUniformDegreeTreeIterator {
public: 
   // filter == NULL enumerate all entries
   // otherwise: only entries with filter->fct(e) == true
   ANodeIterator(const ANode *root,
		     const ANodeFilter* filter = NULL,
		     bool leavesOnly = false,
		     TraversalOrder torder = PreOrder)
     : NonUniformDegreeTreeIterator(root, torder, 
			(leavesOnly) ? NON_UNIFORM_DEGREE_TREE_ENUM_LEAVES_ONLY
				 : NON_UNIFORM_DEGREE_TREE_ENUM_ALL_NODES),
       filter(filter)
  {
  }


  // Note: really ANode
  virtual NonUniformDegreeTreeNode* 
  Current() const
  {
    NonUniformDegreeTreeNode* s;
    ANode* si = NULL;
    while ( (s = NonUniformDegreeTreeIterator::Current()) ) {
      si = PST_USELESS_DCAST(ANode*, s);
      if ((filter == NULL) || filter->Apply(*si)) { 
	break; 	
      }
      ((ANodeIterator*)this)->operator++();
    } 
    return PST_USELESS_DCAST(ANode*, s);
  }

  
  ANode* 
  CurScope() const 
  { 
    return PST_USELESS_DCAST(ANode*, Current());
  }

private: 
  const ANodeFilter* filter;
};  


//***************************************************************************
// ANodeLineSortedIterator
//
// ANodeLineSortedChildIterator
//    behaves as ANodeIterator (PreOrder), 
//    except that it gurantees LineOrder among siblings  
//
// LineOrder: ACodeNode* a is enumerated before b 
//                    iff a->StartLine() < b->StartLine() 
//            RefInfo * a is enumerated before b 
//                    iff a->StartPos() < b->StartPos() 
//
// NOTE: the implementation was generalized so that it no longer assumes
//       that children in the tree contain non-overlapping ranges. all
//       lines are gathered into a set, sorted, and then enumerated out
//       of the set in sorted order. -- johnmc 5/31/00
//***************************************************************************

class ANodeLineSortedIterator {
public: 
  ANodeLineSortedIterator(const ACodeNode *file, 
			      const ANodeFilter* filterFunc = NULL, 
			      bool leavesOnly = true);
  ~ANodeLineSortedIterator();
  
  ACodeNode* Current() const
  {
    ACodeNode *cur = NULL;
    if (ptrSetIt->Current()) {
      cur = (ACodeNode*) (*ptrSetIt->Current());
    }
    return cur;
  } 


  void  operator++(int)   { (*ptrSetIt)++;}

  void Reset()
  {
    ptrSetIt->Reset();
  }

  void DumpAndReset(std::ostream &os = std::cerr);

private:
  WordSet scopes;  // the scopes we want to have sorted
  WordSetSortedIterator *ptrSetIt;  
};

class ANodeLineSortedChildIterator {
public: 
  ANodeLineSortedChildIterator(const ANode *root, 
				   const ANodeFilter* filterFunc = NULL);
  ~ANodeLineSortedChildIterator();

  ACodeNode* Current() const
  {
    ACodeNode *cur = NULL;
    if (ptrSetIt->Current()) {
      cur = (ACodeNode*) (*ptrSetIt->Current());
    }
    return cur;
  }

  void  operator++(int)   { (*ptrSetIt)++;}

  void Reset()
  {
    ptrSetIt->Reset();
  }

  void DumpAndReset(std::ostream &os = std::cerr);

  // both of these are buried in other parts of the HPCView code :-(
  ACodeNode* CurScope() const { return dynamic_cast<ACodeNode*>(Current()); }
  ACodeNode* CurCode()  const { return dynamic_cast<ACodeNode*>(Current()); }

private:
  WordSet scopes;  // the scopes we want to have sorted
  WordSetSortedIterator* ptrSetIt;
};

//***************************************************************************
// ACodeNodeLine
//  is a dummy class used just for the large scopes iterators.
//  the instance can be created either for the start line or for the end line
//
// FIXME: do we need this?
//***************************************************************************

#define IS_BEG_LINE 0
#define IS_END_LINE 1

class ACodeNodeLine
{
public:
  ACodeNodeLine( ACodeNode* ci, int forEndLine ) 
    : _ci(ci), _forEndLine(forEndLine), _type(ci->Type())
  {
  }
  
  int IsEndLine()    { return _forEndLine; }
  
  ACodeNode* GetACodeNode()  { return _ci; }
  
  SrcFile::ln GetLine()
  { 
      return (_forEndLine ? _ci->endLine() : _ci->begLine());
  }
  
  ANode::ANodeTy Type()   { return _type; }
  
private:
  ACodeNode* _ci;
  int _forEndLine;
  ANode::ANodeTy _type;
};

//***************************************************************************
// ANodeLineSortedIteratorForLargeScopes
//
//   behaves as ANodeLineSortedIterator, but it consider both the
//   StartLine and the EndLine for scopes such as LOOPs or PROCs or
//   even GROUPs.
//    -- mgabi 08/18/01
//   it gurantees LineOrder among siblings  
//
// LineOrder: ACodeNode* a is enumerated before b 
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

class ANodeLineSortedIteratorForLargeScopes {
public: 
  ANodeLineSortedIteratorForLargeScopes(const ACodeNode *file, 
			      const ANodeFilter *filterFunc = NULL, 
			      bool leavesOnly = true);
  ~ANodeLineSortedIteratorForLargeScopes();
  
  ACodeNodeLine* Current() const;
  void  operator++(int)   { (*ptrSetIt)++;}
  void Reset();
  void DumpAndReset(std::ostream &os = std::cerr);

private:
  WordSet scopes;  // the scopes we want to have sorted
  WordSetSortedIterator *ptrSetIt;  
};

//***************************************************************************
// ANodeNameSortedChildIterator
//    behaves as ANodeChildIterator, except that it gurantees NameOrder 
//    NameOrder: a is enumerated before b iff a->Name() < b->Name() 
//***************************************************************************

class ANodeNameSortedChildIterator {
public: 
  ANodeNameSortedChildIterator(const ANode *root,
				   const ANodeFilter* filterFunc = NULL);
  ~ANodeNameSortedChildIterator();
  
  ACodeNode* Current() const;
  void  operator++(int)   { (*ptrSetIt)++;}
  void Reset();

private:
  static int CompareByName(const void *a, const void *b);
  WordSet scopes;  // the scopes we want to have sorted
  WordSetSortedIterator *ptrSetIt;  
};

//***************************************************************************
// ANodeMetricSortedIterator
//    behaves as ANodeIterator, except that it gurantees PerformanceOrder
//    PerformanceOrder:
//          a is enumerated before b
//          iff a->PerfInfos().GetVal(i) > b->PerfInfos().GetVal(i)
//             where GetVal(i) retrieves the value for the performace
//             statistic that the iterator is to sort for
//
//***************************************************************************

class ANodeMetricSortedIterator {
public:
  ANodeMetricSortedIterator(const Pgm* scope, 
			    uint metricId, 
			    const ANodeFilter* filterFunc = NULL);

  virtual ~ANodeMetricSortedIterator()
  {
    delete ptrSetIt;
  }  
  
  ANode* Current() const
  {
    ANode* cur = NULL;
    if (ptrSetIt->Current()) {
      cur = (ANode*) (*ptrSetIt->Current());
    }
    return cur;
  }

  void operator++(int) { (*ptrSetIt)++; }

  void Reset() { ptrSetIt->Reset(); }

private:
  uint m_metricId; // sort key

  WordSet scopes;  // the scopes we want to have sorted
  WordSetSortedIterator* ptrSetIt;
};


//***************************************************************************
// ANodeMetricSortedChildIterator
//    behaves as ANodeChildIterator, except that it guarantees children in 
//    PerformanceOrder.
//
//    PerformanceOrder:
//          a is enumerated before b
//          iff a->PerfInfos().GetVal(i) > b->PerfInfos().GetVal(i)
//             where GetVal(i) retrieves the value for the performace
//             statistic that the iterator is to sort for
//
//***************************************************************************

class ANodeMetricSortedChildIterator {
public:
  ANodeMetricSortedChildIterator(const ANode *scope, int flattenDepth,
				     int compare(const void* a, const void *b),
				     const ANodeFilter *filterFunc = NULL);
  
  virtual ~ANodeMetricSortedChildIterator()
  {
    delete ptrSetIt;
  }

  ANode* Current() const
  {
    ANode* cur = NULL;
    if (ptrSetIt->Current()) {
      cur = (ANode*) (*ptrSetIt->Current());
    }
    return cur;
  }

  void operator++(int)   { (*ptrSetIt)++;}

  void Reset() { ptrSetIt->Reset(); }

private:
  void AddChildren(const ANode *scope, int curDepth,
		   const ANodeFilter *filterFunc);
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
//***************************************************************************

int CompareByMetricId(const void* a, const void *b);
extern int CompareByMetricId_mId;

} // namespace Struct
} // namespace Prof

#endif /* prof_juicy_Prof_Struct_TreeIterator_hpp */
