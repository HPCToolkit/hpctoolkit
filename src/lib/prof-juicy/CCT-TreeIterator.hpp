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

#ifndef prof_juicy_Prof_CCT_TreeIterator_hpp
#define prof_juicy_Prof_CCT_TreeIterator_hpp

//************************* System Include Files ****************************

#include <iostream>

//*************************** User Include Files ****************************

#include <include/general.h>               

// NOTE: included by CCT-Tree.hpp
//#include "CCT-Tree.hpp"

#include <lib/support/NonUniformDegreeTree.hpp>
#include <lib/support/PtrSetIterator.hpp>

//*************************** Forward Declarations ***************************

namespace Prof {

namespace CCT {


//*****************************************************************************
// ANodeFilter: Filters are used in iterators to make sure only
//   desirable ANodes are iterated
// NodeTypeFilter is an array of globally defined filters that return true 
//   for ANodes with a given type only 
// HasNodeType: global fct used in filters from NodeTypeFilter array
//*****************************************************************************

typedef bool (*ANodeFilterFct)(const ANode& x, long addArg);

class ANodeFilter {
public:
  ANodeFilter(ANodeFilterFct f, const char* n, long a)
    : fct(f), arg(a), name(n) { }
  virtual ~ANodeFilter() { }

  bool Apply(const ANode& s) const { return fct(s, arg); }

  const char* GetName() const { return name; }

private:
   const ANodeFilterFct fct; 
   long arg; 
   const char* name; 
};

// HasNodeType(s,tp) == ((tp == ANY) || (s.Type() == tp));
extern bool HasNodeType(const ANode &sinfo, long type); 

// NodeTypeFile[tp].Apply(s) == HasNodeType(s,tp) 
extern const ANodeFilter NodeTypeFilter[ANode::TyNUMBER];


//*****************************************************************************
// ANodeChildIterator: Iterates over all children of a ANode
//   that pass the given filter; a NULL filter matches all children.
//   No particular order is guaranteed.
//*****************************************************************************
class ANodeChildIterator : public NonUniformDegreeTreeNodeChildIterator {
public: 
  // If filter == NULL enumerate all entries; otherwise, only entries
  // with filter->fct(e) == true
  ANodeChildIterator(const ANode* root, const ANodeFilter* filter = NULL); 
  virtual NonUniformDegreeTreeNode* Current() const; // really ANode
  
  ANode* CurNode() const { return dynamic_cast<ANode*>(Current()); }
private: 
   const ANodeFilter* filter; 
};  


//*****************************************************************************
// ANodeIterator: Iterates over all ANodes in the tree
//   rooted at a given ANode that pass the given filter; a NULL
//   filter matches all nodes.  No particular order is guaranteed,
//   unless stated explicitly, i.e. the default value for 'torder' is
//   changed.
//*****************************************************************************

class ANodeIterator : public NonUniformDegreeTreeIterator {
public: 
  // If filter == NULL enumerate all entries; otherwise, only entries
  // with filter->fct(e) == true
  ANodeIterator(const ANode* root, 
		const ANodeFilter* filter = NULL, 
		bool leavesOnly = false, 
		TraversalOrder torder = PreOrder); 
  
  virtual NonUniformDegreeTreeNode* Current() const; // really ANode
  
  ANode* CurNode() const { return dynamic_cast<ANode*>(Current()); }
private: 
  const ANodeFilter* filter; 
};  


//*****************************************************************************
// ANodeSortedIterator
//
// ANodeSortedChildIterator
//
// cmpByLine: ANode* a is enumerated before b 
//                    iff a->StartLine() < b->StartLine() 
//
// NOTE: the implementation was generalized so that it no longer assumes
//       that children in the tree contain non-overlapping ranges. all
//       lines are gathered into a set, sorted, and then enumerated out
//       of the set in sorted order. -- johnmc 5/31/00
//*****************************************************************************

class ANodeSortedIterator {
public:
  // return -1, 0, or 1 for x < y, x = y, or x > y, respectively
  typedef int (*cmp_fptr_t) (const void* x, const void* y);

  static int cmpByName(const void* x, const void* y);
  static int cmpByLine(const void* y, const void* y);
  static int cmpByStructureId(const void* x, const void* y);

public: 
  ANodeSortedIterator(const ANode* node, 
		      cmp_fptr_t compare_fn,
		      const ANodeFilter* filterFunc = NULL, 
		      bool leavesOnly = true);
  
  ~ANodeSortedIterator();
  
  ANode* Current() const; 
  void operator++(int)   { (*ptrSetIt)++; }
  void Reset(); 
  void DumpAndReset(std::ostream &os = std::cerr); 

private:
  WordSet scopes;  // the scopes we want to have sorted
  WordSetSortedIterator* ptrSetIt;
};


class ANodeSortedChildIterator {
public: 
  ANodeSortedChildIterator(const ANode* root,
			   ANodeSortedIterator::cmp_fptr_t compare_fn,
			   const ANodeFilter* filterFunc = NULL);
  ~ANodeSortedChildIterator(); 
  
  ANode* Current() const; 
  void operator++(int)   { (*ptrSetIt)++; }
  void Reset();
  void DumpAndReset(std::ostream &os = std::cerr);

private:
  WordSet scopes;  // the scopes we want to have sorted
  WordSetSortedIterator* ptrSetIt;  
};


} // namespace CCT

} // namespace Prof

//***************************************************************************

#endif /* prof_juicy_Prof_CCT_TreeIterator_hpp */
