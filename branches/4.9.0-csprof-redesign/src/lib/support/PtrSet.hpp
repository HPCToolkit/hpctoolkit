// -*-Mode: C++;-*-
// $Id$

// * BeginRiceCopyright *****************************************************
// 
// Copyright ((c)) 2002-2007, Rice University 
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

#ifndef PtrSet_H
#define PtrSet_H

//*************************************************************************** 
//
// PtrSet.H
//                                                                          
// Author:  John Mellor-Crummey                             January 1994
//
//***************************************************************************

//************************** System Include Files ***************************

//*************************** User Include Files ****************************

#include "WordSet.hpp"

//*************************** Forward Declarations **************************

template<class T> class PtrSetIterator;

//***************************************************************************

//-------------------------------------------------------------
//  template<class T> class PtrSet
//-------------------------------------------------------------
template<class T> class PtrSet : private WordSet {
public:
  // constructors
  PtrSet() { }
  PtrSet(const PtrSet<T>& rhs) : WordSet(rhs) { } 
    // quick coercion -- for use w/ other other tmpls
  PtrSet(const WordSet& rhs) : WordSet(rhs) { }
    
    
  // destructors
  virtual ~PtrSet() { }
  
  void Add(const T item)
    { WordSet::Add((unsigned long) item); } 
  void Delete(const T item)
    { WordSet::Delete((unsigned long) item); }
  int IsMember(const T item) const
    { return WordSet::IsMember((unsigned long) item); }
  
  void DumpContents(const char *itemName);

  T GetEntryByIndex(unsigned int indx)
    { return (T)WordSet::GetEntryByIndex(indx); }

  int operator==(const PtrSet<T>& rhs)        // equality
    { return WordSet::operator==(rhs); }
  PtrSet<T>& operator|=(const PtrSet<T>& rhs) // union
    { WordSet::operator|=(rhs); return *this; }
  PtrSet<T>& operator&=(const PtrSet<T>& rhs) // intersection
    { WordSet::operator&=(rhs); return *this; }
  PtrSet<T>& operator-=(const PtrSet<T>& rhs) // difference
    { WordSet::operator-=(rhs); return *this; }
  PtrSet<T>& operator=(const PtrSet<T>& rhs)  // copy
    { WordSet::operator=(rhs); return *this; }
  
  WordSet::NumberOfEntries;
  WordSet::Clear;
#if 0
  WordSet::ReadWordSet;
  WordSet::WriteWordSet;
#endif
  
private:
//-------------------------------------------------------------
// friend declaration required so HashTableIterator can be
// used with the private base class
//-------------------------------------------------------------
friend class PtrSetIterator<T>;
};


template<class T> class DestroyablePtrSet : public PtrSet<T> {
public:
  DestroyablePtrSet() : PtrSet<T>() { }
  DestroyablePtrSet(const PtrSet<T>& rhs) : PtrSet<T>(rhs) { }
    // quick coercion -- for use w/ other other tmpls
  DestroyablePtrSet(const WordSet& rhs) : PtrSet<T>(rhs) { }

  void DestroyContents() {
    PtrSetIterator<T> items((PtrSet<T> *)this);
    T item;
    for (; item = items.Current(); items++) {
      Delete(item);			// toss item out of the set first
      delete item;			// and then free its storage
    }
  }
};

#endif /* PtrSet_H */
