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

#include <include/uint.h>

// NOTE: included by CCT-Tree.hpp
//#include "CCT-Tree.hpp"

#include <lib/support/NonUniformDegreeTree.hpp>
#include <lib/support/PtrSetIterator.hpp>

//*************************** Forward Declarations **************************

// tallent: cf. Struct-TreeIterator.hpp
#define PROF_CCT_TREE__USE_DYNAMIC_CAST 0
#if (PROF_CCT_TREE__USE_DYNAMIC_CAST)
# define CCT_USELESS_DCAST(t, x) dynamic_cast<t>(x)
#else
# define CCT_USELESS_DCAST(t, x) (t)(x)
#endif


//***************************************************************************

namespace Prof {

namespace CCT {

//***************************************************************************
// ANodeFilter
//***************************************************************************

typedef bool (*ANodeFilterFct)(const ANode& x, long addArg);

class ANodeFilter {
public:
  ANodeFilter(ANodeFilterFct f, const char* n, long a) 
    : fct(f), name(n), arg(a) 
  { }
  
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

// ANodeTyFilter[tp].Apply(s) == HasANodeTy(s,tp)
extern const ANodeFilter ANodeTyFilter[ANode::TyNUMBER];


//*****************************************************************************
// ANodeChildIterator: Iterates over all children of a ANode that pass
//   the given filter; a NULL filter matches all children.  No
//   particular order is guaranteed.
//*****************************************************************************

// NOTE: To support insertion and deletion of nodes during the lifetime of a
// ANodeChildIterator using Link() and Unlink(), use *reverse* iteration.
//   Link()   : new_node->LinkAfter(node_parent->LastChild())
//   Unlink() : won't disturb FirstChild() pointer (until one node is left)

class ANodeChildIterator : public NonUniformDegreeTreeNodeChildIterator {
public: 

  // If filter == NULL enumerate all entries; otherwise, only entries
  // with filter->fct(e) == true
  ANodeChildIterator(const ANode* root, const ANodeFilter* _filter = NULL)
    : NonUniformDegreeTreeNodeChildIterator(root, /*forward*/false), 
      filter(_filter)
  { }


  // NOTE: really ANode
  virtual NonUniformDegreeTreeNode* 
  Current() const
  {
    NonUniformDegreeTreeNode* x_base;
    ANode* x = NULL;
    while ( (x_base = NonUniformDegreeTreeNodeChildIterator::Current()) ) {
      x = CCT_USELESS_DCAST(ANode*, x_base);
      if ((filter == NULL) || filter->Apply(*x)) {
	break;
      }
      ((ANodeChildIterator*)this)->operator++();
    } 
    return CCT_USELESS_DCAST(ANode*, x_base);
  }
  

  ANode* 
  CurNode() const 
  {
    return CCT_USELESS_DCAST(ANode*, Current());
  }

private: 
  const ANodeFilter* filter; 
};


//***************************************************************************
// ANodeIterator: Iterates over all ANodes in the tree
//   rooted at a given ANode that pass the given filter; a NULL
//   filter matches all nodes.  No particular order is guaranteed,
//   unless stated explicitly, i.e. the default value for 'torder' is
//   changed.
//***************************************************************************

class ANodeIterator : public NonUniformDegreeTreeIterator {
public: 
  // If filter == NULL enumerate all entries; otherwise, only entries
  // with filter->fct(e) == true
  ANodeIterator(const ANode* root,
		const ANodeFilter* _filter = NULL,
		bool leavesOnly = false,
		TraversalOrder torder = PreOrder)
    : NonUniformDegreeTreeIterator(root, torder, 
				   ((leavesOnly) 
				    ? NON_UNIFORM_DEGREE_TREE_ENUM_LEAVES_ONLY
				    : NON_UNIFORM_DEGREE_TREE_ENUM_ALL_NODES)),
      filter(_filter)
  {
  }


  // Note: really ANode
  virtual NonUniformDegreeTreeNode*
  Current() const
  {
    NonUniformDegreeTreeNode* x_base;
    ANode* x = NULL;
    while ( (x_base = NonUniformDegreeTreeIterator::Current()) ) {
      x = CCT_USELESS_DCAST(ANode*, x_base);
      if ((filter == NULL) || filter->Apply(*x)) { 
	break; 	
      }
      ((ANodeIterator*)this)->operator++();
    } 
    return CCT_USELESS_DCAST(ANode*, x_base);
  }


  ANode* 
  CurNode() const 
  { 
    return CCT_USELESS_DCAST(ANode*, Current());
  }

private: 
  const ANodeFilter* filter;
};  


//*****************************************************************************
// ANodeSortedIterator
//
// ANodeSortedChildIterator
//*****************************************************************************

class ANodeSortedIterator {
public:
  // return -1, 0, or 1 for x < y, x = y, or x > y, respectively
  typedef int (*cmp_fptr_t) (const void* x, const void* y);

  static int cmpByName(const void* x, const void* y);
  static int cmpByLine(const void* x, const void* y);
  static int cmpByStructureId(const void* x, const void* y);

public: 
  ANodeSortedIterator(const ANode* node,
		      cmp_fptr_t compare_fn,
		      const ANodeFilter* filterFunc = NULL,
		      bool leavesOnly = true);

  ~ANodeSortedIterator()
  {
    delete ptrSetIt;
  }

  ANode*
  Current() const
  {
    ANode* cur = NULL;
    if (ptrSetIt->Current()) {
      cur = (ANode*) (*ptrSetIt->Current());
    }
    return cur;
  }

  void operator++(int)
  {
    (*ptrSetIt)++;
  }

  void Reset()
  {
    ptrSetIt->Reset();
  }

  void DumpAndReset(std::ostream &os = std::cerr);

private:
  WordSet scopes;
  WordSetSortedIterator* ptrSetIt;
};


class ANodeSortedChildIterator {
public: 
  ANodeSortedChildIterator(const ANode* root,
			   ANodeSortedIterator::cmp_fptr_t compare_fn,
			   const ANodeFilter* filterFunc = NULL);

  ~ANodeSortedChildIterator()
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

  void operator++(int)
  { (*ptrSetIt)++; }

  void Reset()
  {
    ptrSetIt->Reset();
  }

  void DumpAndReset(std::ostream &os = std::cerr);

private:
  WordSet scopes;
  WordSetSortedIterator* ptrSetIt;
};


} // namespace CCT

} // namespace Prof

//***************************************************************************

#endif /* prof_juicy_Prof_CCT_TreeIterator_hpp */
