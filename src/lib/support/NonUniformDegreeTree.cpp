// -*-Mode: C++;-*-
// $Id$
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

#include "NonUniformDegreeTree.h"
#include "Assertion.h"

//*************************** Forward Declarations **************************

using std::endl;

//***************************************************************************
// class NonUniformDegreeTreeNode interface operations
//***************************************************************************

//-----------------------------------------------
// constructor initializes empty node then links
// it to its parent and siblings (if any)
//-----------------------------------------------
NonUniformDegreeTreeNode::NonUniformDegreeTreeNode
(NonUniformDegreeTreeNode *_parent)
{
  ZeroLinks();
  Link(_parent); // link to parent and siblings if any
}


NonUniformDegreeTreeNode& 
NonUniformDegreeTreeNode::operator=(const NonUniformDegreeTreeNode& other) 
{
  // shallow copy
  if (&other != this) {
    parent       = other.parent;
    children     = other.children;
    next_sibling = other.next_sibling;
    prev_sibling = other.prev_sibling;
    child_count  = other.child_count;
  }
  return *this;
}

//-----------------------------------------------
// links a node to a parent and at the end of the 
// circular doubly-linked list of its siblings 
// (if any)
//-----------------------------------------------
void NonUniformDegreeTreeNode::Link(NonUniformDegreeTreeNode *newParent)
{
  BriefAssertion(this->parent == 0); // can only have one parent
  if (newParent != 0) {
    // children maintained as a doubly linked ring. 
    // a new node is linked at the end of the ring (as a predecessor 
    // of "parent->children") which points to first child in the ring
    
    NonUniformDegreeTreeNode *first_sibling = newParent->children;
    if (first_sibling) LinkAfter(first_sibling->prev_sibling);
    else {
      newParent->children = this; // solitary child
      newParent->child_count++;
      parent = newParent;
    }
  }
}


//-----------------------------------------------
void NonUniformDegreeTreeNode::LinkAfter(NonUniformDegreeTreeNode *sibling)
{
  BriefAssertion(sibling != NULL);
  BriefAssertion(this->parent == NULL); // can only have one parent
  
  this->parent = sibling->parent;
  if (parent) parent->child_count++;
    
  // children maintained as a doubly linked ring. 
    
  // link forward chain
  next_sibling = sibling->next_sibling;
  sibling->next_sibling = this;
      
  // link backward chain
  prev_sibling = sibling;
  next_sibling->prev_sibling = this;
}


//-----------------------------------------------
void NonUniformDegreeTreeNode::LinkBefore(NonUniformDegreeTreeNode *sibling)
{
  BriefAssertion(sibling != NULL);
  LinkAfter(sibling->prev_sibling);
  if (parent && sibling == parent->children) {
    parent->children = this;
  }
}

//-----------------------------------------------
// unlinks a node from a parent and siblings
//-----------------------------------------------
void NonUniformDegreeTreeNode::Unlink()
{
  if (parent != 0) {
    // children maintained as a doubly linked ring. 
    // excise this node from from the ring 
    if (--(parent->child_count) == 0) {
      // current node is removed as only child of parent
      // leaving it linked in a circularly linked list with
      // itself
      parent->children = 0;
    } else {
      // fix link from parent to the ring if necessary
      if (parent->children == this)
	parent->children = next_sibling;
	
      // relink predecessor's forward link
      prev_sibling->next_sibling = next_sibling;
      
      // relink successor's backward link
      next_sibling->prev_sibling = prev_sibling;
      
      // relink own pointers into self-circular configuration
      prev_sibling = this;
      next_sibling = this;
    }
  }
  this->parent = 0;
}


//-----------------------------------------------
// virtual destructor that frees all of its 
// children before freeing itself 
//-----------------------------------------------
NonUniformDegreeTreeNode::~NonUniformDegreeTreeNode()
{
  if (child_count > 0) {
    NonUniformDegreeTreeNode *next, *start = children;
    for (int i = child_count; i-- > 0; ) {
      next = start->next_sibling;
      delete start;
      start = next;
    }
  }
}

unsigned int
NonUniformDegreeTreeNode::AncestorCount() const
{
  unsigned int ancestorCount = 0;
  for (NonUniformDegreeTreeNode* ancestor = parent;
       ancestor;
       ancestor = ancestor->parent) {
    ancestorCount++;
  }
  return ancestorCount;
}

String 
NonUniformDegreeTreeNode::ToString() const
{
   return String("NonUniformDegreeTreeNode: ") + String((unsigned long) this); 
}

//****************************************************************************
// class NonUniformDegreeTreeNodeChildIterator interface operations
//****************************************************************************


NonUniformDegreeTreeNodeChildIterator::NonUniformDegreeTreeNodeChildIterator
(const NonUniformDegreeTreeNode* _parent, bool firstToLast) : 
parent(_parent), currentChild(0), forward(firstToLast)
{
  Reset();
}


NonUniformDegreeTreeNodeChildIterator::~NonUniformDegreeTreeNodeChildIterator()
{
}

NonUniformDegreeTreeNode *NonUniformDegreeTreeNodeChildIterator::Current() const
{
  return currentChild;
}


void *NonUniformDegreeTreeNodeChildIterator::CurrentUpCall() const
{
  return Current();
}


void NonUniformDegreeTreeNodeChildIterator::Reset()
{
  currentChild = forward ? parent->FirstChild() : parent->LastChild();
}


void NonUniformDegreeTreeNodeChildIterator::operator++()
{
  if (currentChild) {
    currentChild = forward
                     ? currentChild->NextSibling()
                     : currentChild->PrevSibling();
    const NonUniformDegreeTreeNode* pastEnd = forward
                                                  ? parent->FirstChild()
                                                  : parent->LastChild();
    if (currentChild == pastEnd) 
      currentChild = 0;
  }
}


void NonUniformDegreeTreeNodeChildIterator::operator++(int)
{
  operator++();
}



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


NonUniformDegreeTreeIterator::~NonUniformDegreeTreeIterator()
{
}


NonUniformDegreeTreeNode *NonUniformDegreeTreeIterator::Current() const
{
  return (NonUniformDegreeTreeNode *) IteratorStack::CurrentUpCall();
}


void *NonUniformDegreeTreeIterator::CurrentUpCall() const
{
  return Current();
}


StackableIterator *NonUniformDegreeTreeIterator::IteratorToPushIfAny
(void *current)
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

  return (node->ChildCount() > 0)
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
    os << Current()->ToString() << endl; 
    (*this)++; 
  } 
  Reset(); 
} 

void 
NonUniformDegreeTreeNodeChildIterator::DumpAndReset(std::ostream& os)
{
  os << "NonUniformDegreeTreeNodeChildIterator: " << endl; 
  while (Current()) {
    os << Current()->ToString() << endl; 
    (*this)++; 
  } 
  Reset(); 
} 
