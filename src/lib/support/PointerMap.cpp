// $Id$
// -*-C++-*-
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

//***********************************************************************//
//    PointerMap.h:                                                      //
//                                                                       //
//      unidirectional pointer translation map: (void*) --> (void*)      //
//                                                                       //
//    Author:                                                            //
//      John Mellor-Crummey                               May 1993       //
//                                                                       //
//                                                                       //
//***********************************************************************//

//************************* System Include Files ****************************

#include <iostream>

//*************************** User Include Files ****************************

#include "PointerMap.h"
#include "Assertion.h"

//*************************** Forward Declarations ***************************

using std::cerr;
using std::endl;

//****************************************************************************

class PointerMapEntry
{
  public: 
    PointerMapEntry(void* src, void* target = NULL)
      : oldPtr(src), newPtr(target) { };
    void * const oldPtr;
    void * const newPtr;
};

/******************* PointerMap Member Functions - Public ********************/

PointerMap::PointerMap(const uint initialSz)
{
  HashTable::Create(sizeof(PointerMapEntry), initialSz); 
} 

PointerMap::~PointerMap()
{
  Destroy();
}

void PointerMap::Destroy()
{
  HashTable::Destroy(); 
}

void AddPtrEntryCallback( void* p1, void*p2, va_list args) 
{
  bool *collision = va_arg(args, bool*);
  *collision = true;  // this function is only called on collision
}

void PointerMap::InsertMapping(void* oldPtr, void* newPtr,
			       bool abortOnCollision)
{
  PointerMapEntry e(oldPtr, newPtr);
  if (abortOnCollision) {
    bool collision = false;
    AddEntry(&e, AddPtrEntryCallback, &collision);
    Assertion(!collision,
	      "PointerMap::InsertMapping: trying to insert a ptr twice",
	      ASSERT_NOTIFY); 
  }
  else {
    AddEntry(&e);
  }
}

void PointerMap::RemoveMapping(void* oldPtr)
{
  PointerMapEntry e(oldPtr);
  DeleteEntry(&e); 
}

void* PointerMap::Map(void* oldPtr) const
{
  PointerMapEntry e(oldPtr);
  PointerMapEntry* qe; 

  qe = (PointerMapEntry *) QueryEntry(&e);
  if (qe != NULL) {
    return qe->newPtr; 
  }  else {
    return NULL; 
  } 
}

void PointerMap::Dump() const
{
  PointerMapIterator iterator(this);

  cerr << "PointerMap:: " << (void*) this << endl; 
  while (!(iterator.CurrentSrc() == 0 && iterator.CurrentTarget () == 0))
  {
    cerr << "   Old pointer: " << iterator.CurrentSrc() 
	 << "   -> New Pointer: " << iterator.CurrentTarget()
	 << endl;

    iterator++;
  }
  cerr << endl; 
}


/******************* PointerMap Member Functions - Protected *****************/
uint PointerMap::PMHashFunct(const void* entry, const uint size)
{
  PointerMapEntry* e = (PointerMapEntry *) entry; 
  return (uint) (((psint) (entry)) % size);
}

int PointerMap::PMEntryCompare(const void* entry1, const void* entry2)
{
  return (entry1 != entry2);
}

void
PointerMap::PMEntryCleanup(void* srcPtr, void* dstPtr)
{
}

/******************* PointerMap Member Functions - Private *******************/
uint PointerMap::HashFunct(const void* entry, const uint size)
{
  PointerMapEntry* e = (PointerMapEntry *) entry; 
  return PMHashFunct(e->oldPtr, size);
}

int PointerMap::EntryCompare(const void* entry1, const void* entry2)
{
  PointerMapEntry* e1 = (PointerMapEntry *) entry1; 
  PointerMapEntry* e2 = (PointerMapEntry *) entry2; 

  return PMEntryCompare(e1->oldPtr, e2->oldPtr );
}

void PointerMap::EntryCleanup(void* entry)
{
  PointerMapEntry* e = (PointerMapEntry *) entry; 
  PMEntryCleanup(e->oldPtr, e->newPtr);
}

//***********************************************************************//
// Implementation of class PointerMapIterator 
//***********************************************************************//

PointerMapIterator::PointerMapIterator(const PointerMap* map) 
  : HashTableIterator(map) 
{
}

PointerMapIterator::~PointerMapIterator()
{
}

// Source pointer of mapped pair
void*  PointerMapIterator::CurrentSrc() const
{
  void *cur; 

  if ( (cur = Current()) ) 
    return ((PointerMapEntry*) cur)-> oldPtr;
  else 
    return NULL; 
}

void* PointerMapIterator::CurrentTarget() const // Target pointer of mapped pair
{
  void *cur; 

  if ( (cur = Current()) ) 
    return ((PointerMapEntry*) cur)-> newPtr;
  else 
    return NULL; 
}

// Postfix interator.
void PointerMapIterator::operator++(int)
{
  HashTableIterator::operator++(0);
}

// Restart from beginning.
void PointerMapIterator::Reset()
{
  HashTableIterator::Reset();
}
