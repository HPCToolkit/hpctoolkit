// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2009, Rice University 
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

#ifndef PtrSetIterator_h
#define PtrSetIterator_h

//*************************************************************************** 
//
// PtrSetIterator.h
//                                                                          
// Author:  John Mellor-Crummey                             February 1995
//
//***************************************************************************

//************************** System Include Files ***************************

//*************************** User Include Files ****************************

#include "PtrSet.hpp"

//*************************** Forward Declarations **************************

//***************************************************************************

//-------------------------------------------------------------
// template<class T> class PtrSetIterator
//-------------------------------------------------------------
template<class T> class PtrSetIterator : private WordSetIterator {
public:
  PtrSetIterator(const PtrSet<T> *theTable)
    : WordSetIterator((const WordSet *) theTable)
  { }
  
  virtual ~PtrSetIterator() { }
  
  T Current() const {
    unsigned long *item = WordSetIterator::Current();
    return (item ? *(T *) item : 0);
  }

  WordSetIterator::operator++;
  WordSetIterator::Reset;
};

//-------------------------------------------------------------
// template<class T> class PtrSetSortedIterator
//-------------------------------------------------------------
template<class T> class PtrSetSortedIterator : private WordSetSortedIterator {
public:
  PtrSetSortedIterator(const PtrSet<T> *theTable, 
                       EntryCompareFunctPtr const EntryCompare);
  ~PtrSetSortedIterator() { }
  T Current() const;
  WordSetSortedIterator::operator++;
  WordSetSortedIterator::Reset;
};

#endif /* PtrSetIterator_h */
