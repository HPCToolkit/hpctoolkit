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
// NonUniformDegreeTree.C
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
//  see NonUniformDegreeTree.h
//
//***************************************************************************

//************************* System Include Files ****************************

//*************************** User Include Files ****************************

#include <include/gcc-attr.h>

#include "NonUniformDegreeTree.hpp"
#include "StrUtil.hpp"
#include "diagnostics.h"

//*************************** Forward Declarations **************************

using std::endl;

//***************************************************************************
// class NonUniformDegreeTreeNode interface operations
//***************************************************************************


//-----------------------------------------------
// links a node to a parent and at the end of the
// circular doubly-linked list of its siblings
// (if any)
//-----------------------------------------------
void NonUniformDegreeTreeNode::link(NonUniformDegreeTreeNode *newParent)
{
  DIAG_Assert(this->m_parent == 0, ""); // can only have one parent
  if (newParent != 0) {
    // children maintained as a doubly linked ring.
    // a new node is linked at the end of the ring (as a predecessor
    // of "m_parent->children") which points to first child in the ring

    NonUniformDegreeTreeNode *first_sibling = newParent->m_children;
    if (first_sibling) linkAfter(first_sibling->m_prev_sibling);
    else {
      newParent->m_children = this; // solitary child
      newParent->m_child_count++;
      m_parent = newParent;
    }
  }
}


//-----------------------------------------------
void NonUniformDegreeTreeNode::linkAfter(NonUniformDegreeTreeNode *sibling)
{
  DIAG_Assert(sibling != NULL, "");
  DIAG_Assert(this->m_parent == NULL, ""); // can only have one parent

  this->m_parent = sibling->m_parent;
  if (m_parent) m_parent->m_child_count++;

  // children maintained as a doubly linked ring.

  // link forward chain
  m_next_sibling = sibling->m_next_sibling;
  sibling->m_next_sibling = this;

  // link backward chain
  m_prev_sibling = sibling;
  m_next_sibling->m_prev_sibling = this;
}


//-----------------------------------------------
void NonUniformDegreeTreeNode::linkBefore(NonUniformDegreeTreeNode *sibling)
{
  DIAG_Assert(sibling != NULL, "");
  linkAfter(sibling->m_prev_sibling);
  if (m_parent && sibling == m_parent->m_children) {
    m_parent->m_children = this;
  }
}


//-----------------------------------------------
// unlinks a node from a parent and siblings
//-----------------------------------------------

void NonUniformDegreeTreeNode::unlink()
{
  if (m_parent != 0) {
    // children maintained as a doubly linked ring.
    // excise this node from from the ring
    if (--(m_parent->m_child_count) == 0) {
      // current node is removed as only child of parent
      // leaving it linked in a circularly linked list with
      // itself
      m_parent->m_children = 0;
    } else {
      // fix link from parent to the ring if necessary
      if (m_parent->m_children == this)
	m_parent->m_children = m_next_sibling;
	
      // relink predecessor's forward link
      m_prev_sibling->m_next_sibling = m_next_sibling;

      // relink successor's backward link
      m_next_sibling->m_prev_sibling = m_prev_sibling;

      // relink own pointers into self-circular configuration
      m_prev_sibling = this;
      m_next_sibling = this;
    }
  }
  this->m_parent = 0;
}


uint
NonUniformDegreeTreeNode::ancestorCount() const
{
  unsigned int ancestorCount = 0;
  for (NonUniformDegreeTreeNode* ancestor = m_parent;
       ancestor;
       ancestor = ancestor->m_parent) {
    ancestorCount++;
  }
  return ancestorCount;
}


uint
NonUniformDegreeTreeNode::maxDepth(uint parentDepth)
{
  uint depth = parentDepth + 1;

  if (isLeaf()) {
    return depth;
  }
  else {
    uint max_depth = 0;
    NonUniformDegreeTreeNodeChildIterator it(this);
    for (NonUniformDegreeTreeNode* x = NULL; (x = it.Current()); ++it) {
      uint x_depth = x->maxDepth(depth);
      max_depth = std::max(max_depth, x_depth);
    }
    return max_depth;
  }
}


std::string
NonUniformDegreeTreeNode::toString(uint GCC_ATTR_UNUSED oFlags,
				   const char* GCC_ATTR_UNUSED pfx) const
{
  return "NonUniformDegreeTreeNode: " + StrUtil::toStr((void*)this);
}


//****************************************************************************
// class NonUniformDegreeTreeNodeChildIterator interface operations
//****************************************************************************



//****************************************************************************
// class NonUniformDegreeTreeIterator interface operations
//****************************************************************************


NonUniformDegreeTreeIterator::NonUniformDegreeTreeIterator(
                                  const NonUniformDegreeTreeNode* root,
				  TraversalOrder torder,
				  NonUniformDegreeTreeEnumType how) :
  IteratorStack(torder, (how == NON_UNIFORM_DEGREE_TREE_ENUM_LEAVES_ONLY)
                            ? ITER_STACK_ENUM_LEAVES_ONLY
		            : ITER_STACK_ENUM_ALL_NODES)
{
  StackableIterator* top = 0;
  if (how == NON_UNIFORM_DEGREE_TREE_ENUM_ALL_NON_ROOTS)
  {// make an iterator for the next (non-root) level
    top = IteratorToPushIfAny((void*) root);
  }
  else
  {// make a singleton iterator for the root
    top = new SingletonIterator(root,
                               (torder == PostOrder) ? PostVisit : PreVisit);
  }
  if (top)
  {// there is something to push on the stack
    Push(top);
  }
}


StackableIterator*
NonUniformDegreeTreeIterator::IteratorToPushIfAny(void *current)
{
  NonUniformDegreeTreeNode *node = (NonUniformDegreeTreeNode *) current;

  if (GetTraversalOrder() == PreAndPostOrder) {
    StackableIterator *top = dynamic_cast<StackableIterator*>(Top());
    SingletonIterator *stop = dynamic_cast<SingletonIterator*>(top);
    if (stop == 0) { // not a SingletonIterator
      Push(new SingletonIterator(node, PostVisit));
    } else {
      if (stop->VisitType() == PreVisit) {
	Push(new SingletonIterator(node, PostVisit));
      } else return 0;
    }
  }

  return (node->childCount() > 0)
       ? new NonUniformDegreeTreeNodeChildIterator(node, IterationIsForward())
       : 0;
}



/**********************************************************************/
/* debugging support                                                  */
/**********************************************************************/
void
NonUniformDegreeTreeIterator::DumpAndReset(std::ostream& os)
{
  os << "NonUniformDegreeTreeIterator: " << endl;
  while (Current()) {
    os << Current()->toString() << endl;
    (*this)++;
  }
  Reset();
}


void
NonUniformDegreeTreeNodeChildIterator::DumpAndReset(std::ostream& os)
{
  os << "NonUniformDegreeTreeNodeChildIterator: " << endl;
  while (Current()) {
    os << Current()->toString() << endl;
    (*this)++;
  }
  Reset();
}
