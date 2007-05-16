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
//    CSProfTreeIterator.h
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef CSProfTreeIterator_h
#define CSProfTreeIterator_h

//************************* System Include Files ****************************

#include <iostream>

//*************************** User Include Files ****************************

#include <include/general.h>               

#include "CSProfTree.hpp"
#include <lib/support/NonUniformDegreeTree.hpp>
#include <lib/support/PtrSetIterator.hpp>

//*************************** Forward Declarations ***************************

//*****************************************************************************
// CSProfNodeFilter: Filters are used in iterators to make sure only
//   desirable CSProfNodes are iterated
// NodeTypeFilter is an array of globally defined filters that return true 
//   for CSProfNodes with a given type only 
// HasNodeType: global fct used in filters from NodeTypeFilter array
//*****************************************************************************

typedef bool (*CSProfNodeFilterFct)(const CSProfNode& x, long addArg);

class CSProfNodeFilter {
public:
  CSProfNodeFilter(CSProfNodeFilterFct f, const char* n, long a)
    : fct(f), arg(a), name(n) { }
  virtual ~CSProfNodeFilter() { }

  bool Apply(const CSProfNode& s) const { return fct(s, arg); }

  const char* GetName() const { return name; }

private:
   const CSProfNodeFilterFct fct; 
   long arg; 
   const char* name; 
};

// HasNodeType(s,tp) == ((tp == ANY) || (s.Type() == tp));
extern bool HasNodeType(const CSProfNode &sinfo, long type); 

// NodeTypeFile[tp].Apply(s) == HasNodeType(s,tp) 
extern const CSProfNodeFilter NodeTypeFilter[CSProfNode::NUMBER_OF_TYPES];


//*****************************************************************************
// CSProfNodeChildIterator: Iterates over all children of a CSProfNode
//   that pass the given filter; a NULL filter matches all children.
//   No particular order is guaranteed.
//*****************************************************************************
class CSProfNodeChildIterator : public NonUniformDegreeTreeNodeChildIterator {
public: 
  // If filter == NULL enumerate all entries; otherwise, only entries
  // with filter->fct(e) == true
  CSProfNodeChildIterator(const CSProfNode* root, 
			  const CSProfNodeFilter* filter = NULL); 
  virtual NonUniformDegreeTreeNode* Current() const; // really CSProfNode
  
  CSProfNode* CurNode() const { return dynamic_cast<CSProfNode*>(Current()); }
private: 
   const CSProfNodeFilter* filter; 
};  


//*****************************************************************************
// CSProfNodeIterator: Iterates over all CSProfNodes in the tree
//   rooted at a given CSProfNode that pass the given filter; a NULL
//   filter matches all nodes.  No particular order is guaranteed,
//   unless stated explicitly, i.e. the default value for 'torder' is
//   changed.
//*****************************************************************************

class CSProfNodeIterator : public NonUniformDegreeTreeIterator {
public: 
  // If filter == NULL enumerate all entries; otherwise, only entries
  // with filter->fct(e) == true
  CSProfNodeIterator(const CSProfNode* root, 
		     const CSProfNodeFilter* filter = NULL, 
                     bool leavesOnly = false, 
		     TraversalOrder torder = PreOrder); 
  
  virtual NonUniformDegreeTreeNode* Current() const; // really CSProfNode
  
  CSProfNode* CurNode() const { return dynamic_cast<CSProfNode*>(Current()); }
private: 
  const CSProfNodeFilter* filter; 
};  


//*****************************************************************************
// CSProfNodeLineSortedIterator
//
// CSProfNodeLineSortedChildIterator
//    behaves as CSProfNodeIterator (PreOrder), 
//    except that it gurantees LineOrder among siblings  
//
// LineOrder: CSProfCodeNode* a is enumerated before b 
//                    iff a->StartLine() < b->StartLine() 
//
// NOTE: the implementation was generalized so that it no longer assumes
//       that children in the tree contain non-overlapping ranges. all
//       lines are gathered into a set, sorted, and then enumerated out
//       of the set in sorted order. -- johnmc 5/31/00
//*****************************************************************************

class CSProfNodeLineSortedIterator {
public: 
  CSProfNodeLineSortedIterator(const CSProfCodeNode* file, 
			       const CSProfNodeFilter* filterFunc = NULL, 
			       bool leavesOnly = true);
  ~CSProfNodeLineSortedIterator();
  
  CSProfCodeNode* Current() const; 
  void operator++(int)   { (*ptrSetIt)++; }
  void Reset(); 
  void DumpAndReset(std::ostream &os = std::cerr); 

private:
  WordSet scopes;  // the scopes we want to have sorted
  WordSetSortedIterator* ptrSetIt;
};

class CSProfNodeLineSortedChildIterator {
public: 
  CSProfNodeLineSortedChildIterator(const CSProfNode* root,
				    const CSProfNodeFilter* filterFunc = NULL);
  ~CSProfNodeLineSortedChildIterator(); 
  
  CSProfCodeNode* Current() const; 
  void operator++(int)   { (*ptrSetIt)++; }
  void Reset();
  void DumpAndReset(std::ostream &os = std::cerr);

private:
  WordSet scopes;  // the scopes we want to have sorted
  WordSetSortedIterator* ptrSetIt;  
};

//*****************************************************************************
// CSProfNodeNameSortedChildIterator
//    behaves as CSProfNodeChildIterator, except that it gurantees NameOrder 
//    NameOrder: a is enumerated before b iff a->Name() < b->Name() 
//*****************************************************************************

class CSProfNodeNameSortedChildIterator {
public: 
  CSProfNodeNameSortedChildIterator(const CSProfNode* root, 
				    const CSProfNodeFilter* filterFunc = NULL);
  ~CSProfNodeNameSortedChildIterator(); 
  
  CSProfCodeNode* Current() const; 
  void operator++(int)   { (*ptrSetIt)++; }
  void Reset(); 

private:
  static int CompareByName(const void *a, const void *b); 
  WordSet scopes;  // the scopes we want to have sorted
  WordSetSortedIterator *ptrSetIt;  
};

//***************************************************************************

#endif 
