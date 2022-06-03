// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2022, Rice University
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
//   $HeadURL$
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef prof_Prof_Struct_TreeIterator_hpp
#define prof_Prof_Struct_TreeIterator_hpp

//************************* System Include Files ****************************

#include <iostream>

//*************************** User Include Files ****************************

#include <include/uint.h>

//NOTE: included by Struct-Tree.hpp
//#include "Struct-Tree.hpp"

#include <lib/support/NonUniformDegreeTree.hpp>
#include <lib/support/WordSet.hpp>

//*************************** Forward Declarations **************************

//***************************************************************************

namespace Prof {

namespace Struct {

//***************************************************************************
// ANodeFilter
//***************************************************************************

typedef bool (*ANodeFilterFct)(const ANode& x, long addArg);

class ANodeFilter {
public:
  ANodeFilter(ANodeFilterFct f, const char* n, long a) 
    : fct(f), m_name(n), arg(a) 
  { }
  
  virtual ~ANodeFilter()
  { }

  bool
  apply(const ANode& s) const
  { return fct(s, arg); }

  const char*
  name() const
  { return m_name; }

private:
  const ANodeFilterFct fct;
  const char* m_name;
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

// NOTE: To support insertion and deletion of nodes during the
// lifetime of a ANodeChildIterator using link() and unlink(), use
// *reverse* iteration.
//   link()   : links newNode after the iteration start point:
//                newNode->linkAfter(newParent->LastChild())
//   unlink() : won't disturb the iteration end point:
//                parent->FirstChild()
//              until the last node is removed

class ANodeChildIterator : public NonUniformDegreeTreeNodeChildIterator {
public: 

  // If filter == NULL enumerate all entries; otherwise, only entries
  // with filter->fct(e) == true
  ANodeChildIterator(const ANode* root, const ANodeFilter* filter = NULL)
    : NonUniformDegreeTreeNodeChildIterator(root, /*forward*/false), 
      m_filter(filter)
  { }


  ANode*
  current() const 
  { return static_cast<ANode*>(Current()); }


  virtual NonUniformDegreeTreeNode*
  Current() const
  {
    NonUniformDegreeTreeNode* x_base = NULL;
    while ( (x_base = NonUniformDegreeTreeNodeChildIterator::Current()) ) {
      ANode* x = static_cast<ANode*>(x_base);
      if ((m_filter == NULL) || m_filter->apply(*x)) {
	break;
      }
      const_cast<ANodeChildIterator*>(this)->operator++();
    }
    return x_base;
  }
  
private: 
  const ANodeFilter* m_filter; 
};


class ACodeNodeChildIterator : public NonUniformDegreeTreeNodeChildIterator {
public: 
  ACodeNodeChildIterator(const ACodeNode* root)
    : NonUniformDegreeTreeNodeChildIterator(root, /*forward*/false)
  { }

  ACodeNode*
  current() const 
  { 
    return dynamic_cast<ACodeNode*>(Current()); 
  }

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
		const ANodeFilter* filter = NULL,
		bool leavesOnly = false,
		TraversalOrder torder = PreOrder)
    : NonUniformDegreeTreeIterator(root, torder, 
				   ((leavesOnly) 
				    ? NON_UNIFORM_DEGREE_TREE_ENUM_LEAVES_ONLY
				    : NON_UNIFORM_DEGREE_TREE_ENUM_ALL_NODES)),
      m_filter(filter)
  { }


  ANode*
  current() const
  { return static_cast<ANode*>(Current()); }


  virtual NonUniformDegreeTreeNode*
  Current() const
  {
    NonUniformDegreeTreeNode* x_base = NULL;
    while ( (x_base = NonUniformDegreeTreeIterator::Current()) ) {
      ANode* x = static_cast<ANode*>(x_base);
      if ((m_filter == NULL) || m_filter->apply(*x)) {
	break;
      }
      const_cast<ANodeIterator*>(this)->operator++();
    } 
    return x_base;
  }

private: 
  const ANodeFilter* m_filter;
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
  static int cmpById(const void* x, const void* y);

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
  current() const
  {
    ANode* x = NULL;
    if (ptrSetIt->Current()) {
      x = reinterpret_cast<ANode*>(*ptrSetIt->Current());
    }
    return x;
  }

  void
  operator++(int)
  {
    (*ptrSetIt)++;
  }

  void
  reset()
  {
    ptrSetIt->Reset();
  }

  void
  dumpAndReset(std::ostream &os = std::cerr);

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

  ANode*
  current() const
  {
    ANode* x = NULL;
    if (ptrSetIt->Current()) {
      x = reinterpret_cast<ANode*>(*ptrSetIt->Current());
    }
    return x;
  }

  void
  operator++(int)
  { (*ptrSetIt)++; }

  void
  reset()
  {
    ptrSetIt->Reset();
  }

  void
  dumpAndReset(std::ostream &os = std::cerr);

private:
  WordSet scopes;
  WordSetSortedIterator* ptrSetIt;
};


} // namespace Struct

} // namespace Prof

//***************************************************************************

#endif /* prof_Prof_Struct_TreeIterator_hpp */
