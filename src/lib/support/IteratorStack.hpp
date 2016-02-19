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

#include <include/uint.h>

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
