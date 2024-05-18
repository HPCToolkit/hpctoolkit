// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

//***************************************************************************
// IteratorStack.C
//
//   an iterator that is realized as a stack of iterators. this abstraction
//   is useful for traversing nested structures.
//
// Author: John Mellor-Crummey
//
// Creation Date: October 1993
//
// Modification History:
//  see IteratorStack.h
//
//***************************************************************************

//************************** System Include Files ***************************

//*************************** User Include Files ****************************

#include "IteratorStack.hpp"
#include "PointerStack.hpp"
#include "diagnostics.h"

//*************************** Forward Declarations **************************

//***************************************************************************

struct IteratorStackS {
  PointerStack pstack;
};


IteratorStack::IteratorStack(TraversalOrder torder,
                             IterStackEnumType _enumType)
{
  iteratorStackRepr = new IteratorStackS;
  InitTraversal(torder, _enumType);
}


IteratorStack::~IteratorStack()
{
  FreeStack(0);
  delete iteratorStackRepr;
}

StackableIterator* IteratorStack::Top() const
{
  return (StackableIterator*) iteratorStackRepr->pstack.Top();
}

StackableIterator *IteratorStack::GetIteratorAtPosition(unsigned int depth) const
{
  return (StackableIterator*) iteratorStackRepr->pstack.Get(depth);
}


void IteratorStack::Push(StackableIterator* newtop)
{
  while (newtop != 0) {
    if (newtop->CurrentUpCall() == 0) {
      // don't really push empty iterators
      delete newtop;
      break;
    }
    iteratorStackRepr->pstack.Push(newtop);
    if (traversalOrder != PostOrder) break;
    newtop = IteratorToPushIfAny(newtop->CurrentUpCall());
  }
}


void IteratorStack::operator++()
{
  (*this)++;
}


void IteratorStack::operator++(int)
{
  for(;;) {
    StackableIterator* top = Top();
    if (top == 0) break;

    if ((traversalOrder == PreOrder) || (traversalOrder == PreAndPostOrder)) {
      void* current = top->CurrentUpCall();
      (*top)++; // advance iterator at the top of stack
      if (current) {
        Push(IteratorToPushIfAny(current));
        top = Top();
      }
    }
    else
      (*top)++; // advance iterator at the top of stack

    if (top->IsValid() == false) {
      bool popped = false;
      while ((Top()->IsValid() == false) &&
             (iteratorStackRepr->pstack.Depth() > 1)) {
        FreeTop();
        popped = true;
      }
      if (popped && (enumType == ITER_STACK_ENUM_LEAVES_ONLY))
        continue;
    } else if (traversalOrder == PostOrder) {
      void* current = top->CurrentUpCall();
      if (current) {
        Push(IteratorToPushIfAny(current));
      }
    }
    break;
  }
}


void IteratorStack::ReConstruct(TraversalOrder torder,
                                IterStackEnumType _enumType)
{
  InitTraversal(torder, _enumType);
  FreeStack(0);
}


void IteratorStack::Reset()
{
  FreeStack(1); // leave at most the top element on stack
  StackableIterator* top = Top();
  if (top) {
    top->Reset();
    if (traversalOrder == PostOrder)
      Push(IteratorToPushIfAny(top->CurrentUpCall()));
  }
}

void IteratorStack::Reset(TraversalOrder torder, IterStackEnumType _enumType)
{
  InitTraversal(torder, _enumType);
  Reset();
}


void* IteratorStack::CurrentUpCall() const
{
  StackableIterator* top = Top();
  return (top ? top->CurrentUpCall() : 0);
}


bool IteratorStack::IsValid() const
{
  StackableIterator* top = Top();
  return (top ? top->IsValid() : false);
}

TraversalVisitType IteratorStack::VisitType() const
{
  switch(clientTraversalOrder) {
  case PreOrder:
  case ReversePreOrder:
    return PreVisit;
  case PostOrder:
  case ReversePostOrder:
    return PostVisit;
//case ReversePreAndPostOrder:
  case PreAndPostOrder: {
    StackableIterator* top = dynamic_cast<StackableIterator*>(Top());
    SingletonIterator* stop = dynamic_cast<SingletonIterator*>(top);
    if (top == 0) {
      DIAG_Die("");
    } else if (stop != 0) {
      return stop->VisitType();
    } else {
      return PreVisit;
    }
  }
  default:
    DIAG_Die("");
  }
  return PostVisit;  // bogus return--not reached
}

IteratorStack::TraversalOrder IteratorStack::GetTraversalOrder() const
{
  return clientTraversalOrder;
}

bool IteratorStack::IterationIsForward() const
{
  switch(clientTraversalOrder) {
  case PreOrder:
  case PostOrder:
  case PreAndPostOrder:
    return true;
  case ReversePreOrder:
  case ReversePostOrder:
//case ReversePreAndPostOrder:
    return false;
  default:
    DIAG_Die("");
  }
  return false;  // bogus return--not reached
}

int IteratorStack::Depth() const
{
  return iteratorStackRepr->pstack.Depth();
}


void IteratorStack::FreeTop()
{
  StackableIterator* top= (StackableIterator*) iteratorStackRepr->pstack.Pop();
  if (top)
    delete top;
}


// free the top (depth - maxDepth) elements on the stack, leaving at
// most maxDepth elements on the stack: FreeStack(1) leaves at most one
// element on the stack
void IteratorStack::FreeStack(int maxDepth)
{
  int depth = iteratorStackRepr->pstack.Depth();
  while (depth-- > maxDepth)
    FreeTop();
}


void IteratorStack::InitTraversal(TraversalOrder torder,
                                  IterStackEnumType _enumType)
{
  clientTraversalOrder = torder;
  enumType = _enumType;
  if (enumType == ITER_STACK_ENUM_LEAVES_ONLY)
    traversalOrder = PostOrder;
  else if (torder == ReversePreOrder)
    traversalOrder = PreOrder;  // reversed by IteratorToPushIfAny
  else if (torder == ReversePostOrder)
    traversalOrder = PostOrder;  // reversed by IteratorToPushIfAny
//else if (torder == ReversePreAndPostOrder)
//  traversalOrder = PreAndPostOrder;  // reversed by IteratorToPushIfAny
  else {
    DIAG_Assert((torder == PreOrder) || (torder == PostOrder) ||
                (torder == PreAndPostOrder), "");
    traversalOrder = torder;
  }
}


void IteratorStack::DumpUpCall()
{
  //dumpHandler.BeginScope();
  int depth = iteratorStackRepr->pstack.Depth();
  for (; --depth >= 0; ) {
    StackableIterator* it =
      (StackableIterator*) iteratorStackRepr->pstack.Get(depth);
    it->Dump();
  }
  //dumpHandler.EndScope();
}



//****************************************************************************
// class SingletonIterator
//****************************************************************************

SingletonIterator::SingletonIterator(const void* singletonValue,
                                     TraversalVisitType vtype)
  : value(singletonValue), done(false), visitType(vtype)
{
}


SingletonIterator::~SingletonIterator()
{
}


void* SingletonIterator::CurrentUpCall() const
{
  const void* retval = done ? 0 : value;
  return (void*) retval;  // const cast away
}


void SingletonIterator::operator++()
{
  done = true;
}


void SingletonIterator::operator++(int)
{
  done = true;
}


void SingletonIterator::Reset()
{
  done = false;
}


TraversalVisitType SingletonIterator::VisitType() const
{
  return visitType;
}
