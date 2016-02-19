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
// Copyright ((c)) 2002-2016, Rice University
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
// NonUniformDegreeTree.h
//
//   a general purpose abstraction for non-uniform degree trees.
//   children of a node are represented by a circularly linked-list
//   of siblings.
//
//   two iterators are included here. one for enumerating the children
//   of a node, and a second for enumerating a tree rooted at a node.
//
//   these abstractions are useless in their own right since the tree
//   contains only structural information. to make use of the abstraction,
//   derive a tree node class that contains some useful data. all of the
//   structural manipulation can be performed using the functions provided
//   in the base classes defined here.
//
// Author: John Mellor-Crummey
//
// Creation Date: September 1991
//
// Modification History:
//  November 1994 -- John Mellor-Crummey
//     -- reimplemented iterators in terms of StackableIterator and
//        IteratorStack, increasing functionality while reducing code
//        volume.
//
//***************************************************************************

#ifndef support_NonUniformDegreeTree_hpp
#define support_NonUniformDegreeTree_hpp

//************************* System Include Files ****************************

#include <iostream>
#include <string>

//*************************** User Include Files ****************************

#include <include/uint.h>

#include "IteratorStack.hpp"

//*************************** Forward Declarations **************************

//***************************************************************************
// class NonUniformDegreeTreeNode
//***************************************************************************

class NonUniformDegreeTreeNode {
public:

  //-----------------------------------------------
  // constructor initializes empty node then links
  // it to its parent and siblings (if any)
  //-----------------------------------------------
  NonUniformDegreeTreeNode(NonUniformDegreeTreeNode* parent = 0)
  {
    zeroLinks();
    link(parent); // link to parent and siblings if any
  }

  NonUniformDegreeTreeNode(const NonUniformDegreeTreeNode& other)
  {
    *this = other;
  }

  NonUniformDegreeTreeNode&
  operator=(const NonUniformDegreeTreeNode& other)
  {
    // shallow copy
    if (&other != this) {
      m_parent       = other.m_parent;
      m_children     = other.m_children;
      m_next_sibling = other.m_next_sibling;
      m_prev_sibling = other.m_prev_sibling;
      m_child_count  = other.m_child_count;
    }
    return *this;
  }

  //-----------------------------------------------
  // virtual destructor that frees all of its
  // children before freeing itself
  //-----------------------------------------------
  virtual ~NonUniformDegreeTreeNode()
  {
    if (m_child_count > 0) {
      NonUniformDegreeTreeNode *next, *start = m_children;
      for (int i = m_child_count; i-- > 0; ) {
	next = start->m_next_sibling;
	delete start;
	start = next;
      }
    }
  }

  // link/unlink a node to a parent and siblings
  void
  link(NonUniformDegreeTreeNode *parent);

  void
  linkBefore(NonUniformDegreeTreeNode *sibling);

  void
  linkAfter(NonUniformDegreeTreeNode *sibling);

  void
  unlink();

  // returns the number of ancestors walking up the tree
  uint
  ancestorCount() const;

  // functions for inspecting links to other nodes
  uint
  childCount() const
  { return m_child_count; };

  bool
  isLeaf() const
  { return (m_child_count == 0); }


  uint
  maxDepth()
  { return maxDepth(0); }

  uint
  maxDepth(uint parentDepth);

public:
  virtual std::string
  toString(uint oFlags = 0, const char* pfx = "") const;

public:
  // N.B.: For derived classes, these may get in the way...
  NonUniformDegreeTreeNode*
  Parent() const
  { return m_parent; };

  NonUniformDegreeTreeNode*
  NextSibling() const
  { return m_next_sibling; };

  NonUniformDegreeTreeNode*
  PrevSibling() const
  { return m_prev_sibling; };

  NonUniformDegreeTreeNode*
  FirstChild() const
  { return m_children; };

  NonUniformDegreeTreeNode*
  LastChild() const
  { return m_children ? m_children->m_prev_sibling : 0; };

protected:
  // useful for resetting parent/child/etc links after cloning
  void
  zeroLinks()
  {
    // no parent
    m_parent = NULL;

    // no children
    m_children = NULL;
    m_child_count = 0;

    // initial circular list of siblings includes only self
    m_next_sibling = m_prev_sibling = this;
  }

protected:
  NonUniformDegreeTreeNode* m_parent;
  NonUniformDegreeTreeNode* m_children;
  NonUniformDegreeTreeNode* m_next_sibling;
  NonUniformDegreeTreeNode* m_prev_sibling;
  uint m_child_count;

  friend class NonUniformDegreeTreeNodeChildIterator;
  friend class NonUniformDegreeTreeIterator;
};



//***************************************************************************
// class NonUniformDegreeTreeNodeChildIterator
//***************************************************************************

class NonUniformDegreeTreeNodeChildIterator : public StackableIterator {
public:
  NonUniformDegreeTreeNodeChildIterator(const NonUniformDegreeTreeNode* parent,
					bool _forward = true)
    : m_parent(parent), currentChild(0), forward(_forward)
  {
    Reset();
  }

  ~NonUniformDegreeTreeNodeChildIterator()
  {
  }

  void Reset(void)
  {
    currentChild = forward ? m_parent->FirstChild() : m_parent->LastChild();
  }

  // prefix increment
  void
  operator++()
  {
    if (currentChild) {
      currentChild = (forward ? currentChild->NextSibling()
   		              : currentChild->PrevSibling());
      const NonUniformDegreeTreeNode* pastEnd =
	forward ? m_parent->FirstChild() : m_parent->LastChild();
      if (currentChild == pastEnd) {
	currentChild = NULL;
      }
    }
  }

  // postfix increment
  void
  operator++(int)
  {
    operator++();
  }

  virtual NonUniformDegreeTreeNode*
  Current() const
  {
    return currentChild;
  }

  virtual void
  DumpAndReset(std::ostream &os = std::cerr);

private:
  // interface for StackableIterator
  void*
  CurrentUpCall() const
  {
    return Current();
  }

  const NonUniformDegreeTreeNode *m_parent;
  NonUniformDegreeTreeNode *currentChild;
  bool forward;
};


//***************************************************************************
// class NonUniformDegreeTreeIterator
//***************************************************************************

enum NonUniformDegreeTreeEnumType {
  NON_UNIFORM_DEGREE_TREE_ENUM_LEAVES_ONLY = ITER_STACK_ENUM_LEAVES_ONLY,
  NON_UNIFORM_DEGREE_TREE_ENUM_ALL_NODES = ITER_STACK_ENUM_ALL_NODES,
  NON_UNIFORM_DEGREE_TREE_ENUM_ALL_NON_ROOTS
};

// Note:  Reverse traversal orders are OK.

class NonUniformDegreeTreeIterator : public IteratorStack {
public:
  NonUniformDegreeTreeIterator
    (const NonUniformDegreeTreeNode *root, TraversalOrder torder = PreOrder,
     NonUniformDegreeTreeEnumType how= NON_UNIFORM_DEGREE_TREE_ENUM_ALL_NODES);

  ~NonUniformDegreeTreeIterator()
  {
  }


  virtual NonUniformDegreeTreeNode*
  Current() const
  {
    return (NonUniformDegreeTreeNode *) IteratorStack::CurrentUpCall();
  }

  virtual void DumpAndReset(std::ostream &os = std::cerr);

private:
  // upcall interface for StackableIterator
  void*
  CurrentUpCall() const
  {
    return Current();
  }

  // upcall for IteratorStack
  StackableIterator *
  IteratorToPushIfAny(void *current);
};

#endif /* NonUniformDegreeTree_hpp */
