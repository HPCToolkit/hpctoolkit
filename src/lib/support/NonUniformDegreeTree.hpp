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

#ifndef NonUniformDegreeTree_h
#define NonUniformDegreeTree_h

//************************* System Include Files ****************************

#include <iostream>

//*************************** User Include Files ****************************

#include "IteratorStack.h"
#include "String.h"

//*************************** Forward Declarations **************************

//***************************************************************************
// class NonUniformDegreeTreeNode
//***************************************************************************

class NonUniformDegreeTreeNode {
public:
  NonUniformDegreeTreeNode(NonUniformDegreeTreeNode *_parent = 0);
  
  // destructor for destroying derived class instances
  virtual ~NonUniformDegreeTreeNode();
  
  // link/unlink a node to a parent and siblings 
  void Link(NonUniformDegreeTreeNode *_parent);
  void LinkBefore(NonUniformDegreeTreeNode *sibling);
  void LinkAfter(NonUniformDegreeTreeNode *sibling);
  void Unlink();

  // returns the number of ancestors walking up the tree
  unsigned int AncestorCount() const;
  
  // functions for inspecting links to other nodes
  unsigned int ChildCount() const { return child_count; };

  virtual String ToString() const; 
public:
  NonUniformDegreeTreeNode *Parent() const { return parent; };
  NonUniformDegreeTreeNode *NextSibling() const { return next_sibling; };
  NonUniformDegreeTreeNode *PrevSibling() const { return prev_sibling; };
  NonUniformDegreeTreeNode *FirstChild() const { return children; };
  NonUniformDegreeTreeNode *LastChild() const
    { return children ? children->prev_sibling : 0; };

private:
  NonUniformDegreeTreeNode *parent, *children, *next_sibling, *prev_sibling;
  unsigned int child_count;

friend class NonUniformDegreeTreeNodeChildIterator;
friend class NonUniformDegreeTreeIterator;
};



//***************************************************************************
// class NonUniformDegreeTreeNodeChildIterator
//***************************************************************************

class NonUniformDegreeTreeNodeChildIterator : public StackableIterator {
public:
  NonUniformDegreeTreeNodeChildIterator(
                                     const NonUniformDegreeTreeNode* _parent,
				     bool firstToLast = true);
  ~NonUniformDegreeTreeNodeChildIterator();

  void Reset(void);
  void operator++();    // prefix increment
  void operator++(int); // postfix increment

// protected is commented out because NonUniformDegreeTreeIterators are used
// directly by deprecated (but still necessary) code in the Fortran D compiler.
// it should be uncommented when the offending code is rewritten.
// --JMC 4 Nov 1994
// protected:
  virtual NonUniformDegreeTreeNode *Current() const;

  virtual void DumpAndReset(std::ostream &os = std::cerr);

private:
  void *CurrentUpCall() const; // interface for StackableIterator
  const NonUniformDegreeTreeNode *parent;
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
  ~NonUniformDegreeTreeIterator();

// protected is commented out because NonUniformDegreeTreeIterators are used
// directly by deprecated (but still necessary) code in the Fortran D compiler.
// it should be uncommented when the offending code is rewritten.
// --JMC 4 Nov 1994
// protected:
  virtual NonUniformDegreeTreeNode *Current() const;

  virtual void DumpAndReset(std::ostream &os = std::cerr); 
private:
  // upcall interface for StackableIterator
  void *CurrentUpCall() const; 

  // upcall for IteratorStack
  StackableIterator *IteratorToPushIfAny(void *current); 
};

#endif 
