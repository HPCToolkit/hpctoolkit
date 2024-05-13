// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

//***************************************************************************
// StackableIterator.h
//
//   a base set of functionality for iterators that can be used with the
//   IteratorStack abstraction to traverse nested structures
//
// Author: John Mellor-Crummey
//
// Creation Date: October 1993
//
//***************************************************************************


#ifndef StackableIterator_h
#define StackableIterator_h

//************************** System Include Files ***************************

//*************************** User Include Files ****************************


//*************************** Forward Declarations **************************

//***************************************************************************
// class StackableIterator
//***************************************************************************

class StackableIterator {
public:
  StackableIterator();
  virtual ~StackableIterator();

  //----------------------------------------------------------------------
  // upcall to get the value of the current element in the iteration from
  // a derived iterator
  //----------------------------------------------------------------------
  virtual void *CurrentUpCall() const = 0;

  //----------------------------------------------------------------------
  // upcall to advance the iteration
  //----------------------------------------------------------------------
  virtual void operator++() = 0; // prefix increment
  void operator++(int);          // postincrement via preincrement operator

  //----------------------------------------------------------------------
  // predicate to test if the value returned by CurrentUpCall is valid.
  // supplied default implementation returns true if the value is non-zero
  //----------------------------------------------------------------------
  virtual bool IsValid() const;

  //----------------------------------------------------------------------
  // upcall to restart the iteration at the beginning
  //----------------------------------------------------------------------
  virtual void Reset() = 0;

  //----------------------------------------------------------------------
  // dump essential state (class name, this pointer, current iterate)
  // and invoke DumpUpCall to report interesting state of derived class
  //----------------------------------------------------------------------
  void Dump();

private:
  //----------------------------------------------------------------------
  // upcall to dump any interesting state of a derived class
  //----------------------------------------------------------------------
  virtual void DumpUpCall();
};

#endif
