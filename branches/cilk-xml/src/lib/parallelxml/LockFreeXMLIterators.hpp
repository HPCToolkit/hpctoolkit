/*
 * LockFreeXMLIterators.hpp
 *
 *  Created on: Dec 15, 2014
 *      Author: pat2
 */

#ifndef LIB_PARALLELXML_LOCKFREEXMLITERATORS_HPP_
#define LIB_PARALLELXML_LOCKFREEXMLITERATORS_HPP_

#include "LockFreeXMLNode.hpp"

class LockFreeXMLTreeNodeChildIterator : public StackableIterator {
public:
  LockFreeXMLTreeNodeChildIterator(const LockFreeXMLNode* parent,
					bool _forward = true)
    : m_parent(parent), currentChild(0), forward(_forward)
  {
    Reset();
  }

  ~LockFreeXMLTreeNodeChildIterator()
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
      const LockFreeXMLNode* pastEnd =
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

  virtual LockFreeXMLNode*
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

  const LockFreeXMLNode *m_parent;
  LockFreeXMLNode *currentChild;
  bool forward;
};

// Iterator class
// IT IS NOT SAFE TO USE WHILE THE TREE IS BEING MODIFIED!


enum LockFreeXMLTreeEnumType {
  LOCK_FREE_XML_TREE_ENUM_LEAVES_ONLY = ITER_STACK_ENUM_LEAVES_ONLY,
  LOCK_FREE_XML_TREE_ENUM_ALL_NODES = ITER_STACK_ENUM_ALL_NODES,
  LOCK_FREE_XML_TREE_ENUM_ALL_NON_ROOTS
};

// Note:  Reverse traversal orders are OK.

class LockFreeXMLTreeIterator : public IteratorStack {
public:
	LockFreeXMLTreeIterator
    (const LockFreeXMLNode *root, TraversalOrder torder = PreOrder,
    	LockFreeXMLTreeEnumType how= LOCK_FREE_XML_TREE_ENUM_ALL_NODES);

  ~LockFreeXMLTreeIterator()
  {
  }


  virtual LockFreeXMLNode*
  Current() const
  {
    return (LockFreeXMLNode *) IteratorStack::CurrentUpCall();
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


#endif /* LIB_PARALLELXML_LOCKFREEXMLITERATORS_HPP_ */
