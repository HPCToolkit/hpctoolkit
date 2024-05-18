// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

//***************************************************************************
// IteratorStack.h
//
//   an iterator that is realized as a stack of iterators. this abstraction
//   is useful for traversing nested structures.
//
// Author: John Mellor-Crummey
//
// Creation Date: October 1993
//
// Modification History:
//  November 1994 -- John Mellor-Crummey
//
//**************************************************************************/


#ifndef IteratorStack_h
#define IteratorStack_h

//************************** System Include Files ***************************

//*************************** User Include Files ****************************


#include "StackableIterator.hpp"

//*************************** Forward Declarations **************************

//***************************************************************************

//****************************************************************************
// enumeration type declarations
//***************************************************************************/

enum TraversalVisitType { PreVisit, PostVisit };

enum IterStackEnumType {
  ITER_STACK_ENUM_LEAVES_ONLY, ITER_STACK_ENUM_ALL_NODES
};



//****************************************************************************
// class IteratorStack
//***************************************************************************/

class IteratorStack: public StackableIterator {
public:
  enum TraversalOrder { Unordered, PreOrder, PostOrder,
                        ReversePreOrder, ReversePostOrder,
                        PreAndPostOrder };

public:

  IteratorStack(TraversalOrder torder,
                IterStackEnumType enumType = ITER_STACK_ENUM_ALL_NODES);
  ~IteratorStack();

  void *CurrentUpCall() const;

  void operator++(int); // postfix increment
  void operator++();    // prefix increment

  // pop all but one iterator off the stack; reset the one left
  void Reset(); // same traversal order as before
  void Reset(TraversalOrder torder,
             IterStackEnumType enumType = ITER_STACK_ENUM_ALL_NODES);

  // empty the stack and reset the state to that as if freshly constructed
  void ReConstruct(TraversalOrder torder,
                   IterStackEnumType enumType = ITER_STACK_ENUM_ALL_NODES);

  bool IsValid() const;

  virtual TraversalVisitType VisitType() const;
  TraversalOrder GetTraversalOrder() const;
  virtual bool IterationIsForward() const;

  void DumpUpCall();

protected:
  TraversalOrder clientTraversalOrder; // client supplied traversal order
  IterStackEnumType enumType;

  void Push(StackableIterator *);
  StackableIterator *Top(void) const;
  StackableIterator *GetIteratorAtPosition(unsigned int depth) const; // TOS=0
  void FreeTop();

  int Depth() const;

private:
  virtual StackableIterator *IteratorToPushIfAny(void *current) = 0;

  void FreeStack(int maxDepth);
  void InitTraversal(TraversalOrder torder, IterStackEnumType enumType);

  struct IteratorStackS *iteratorStackRepr;
  TraversalOrder traversalOrder;      // internally computed traversal order
};

//****************************************************************************
// class SingletonIterator
//***************************************************************************/

class SingletonIterator : public StackableIterator  {
public:
  SingletonIterator(const void* singletonValue, TraversalVisitType vtype);
  ~SingletonIterator();

  void* CurrentUpCall() const;
  void operator++();
  void operator++(int);
  void Reset();

  TraversalVisitType VisitType() const;

private:
  const void* value;
  bool done;
  TraversalVisitType visitType;
};


#endif
