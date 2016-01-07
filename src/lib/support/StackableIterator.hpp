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

#include <include/uint.h>

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
  // dump essential state (class name, this pointer, currrent iterate) 
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

