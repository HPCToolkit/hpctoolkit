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

#ifndef support_PointerMap_hpp
#define support_PointerMap_hpp

//***********************************************************************//
//    PointerMap.h:                                                      //
//                                                                       //
//      unidirectional pointer translation map: (void*) --> (void*)      //
//                                                                       //
//      The default hashing fct can be redefined by a derived class by   //
//      implementing PMHashFunct                                         //
//                                                                       //
//      Per default entries are compared by comparing the srcPtrs. This  //
//      can be overwritten by implementing PMEntryCompare.               //
//                                                                       //
//      When entries are removed from the map via RemoveMapping or by    //
//      the Destroy method PMEntryCleanup is being called. Per default   //
//      this method doesn't do anything.                                 //
//                                                                       //
//      eraxxon: Note that because the map is void* to void*, it is not  //
//      safe to use this map for cross platform processing.  However, it //
//      will be safe on the architecture it is compiled for.             //
//                                                                       //
//                                                                       //
//    Author:                                                            //
//      John Mellor-Crummey                               May 1993       //
//                                                                       //
//                                                                       //
//***********************************************************************//

//************************* System Include Files ****************************

//*************************** User Include Files ****************************

#include <include/general.h>

#include "HashTable.hpp"

//*************************** Forward Declarations ***************************

//****************************************************************************

class PointerMap : private HashTable
{
  public:
    PointerMap(const uint initialSz = 8);
    virtual ~PointerMap();

    void  InsertMapping(void* srcPtr, void* targetPtr,
			bool abortOnCollision = true);
    void  RemoveMapping(void* srcPtr);
    void* Map(void* srcPtr) const;                    // 0, if no mapping
    void  Dump() const;
    void  Destroy();

    HashTable::NumberOfEntries;

    friend class PointerMapIterator; 

  protected: 
    // implementation of virtual HashTable functions 
    virtual uint PMHashFunct(const void* entry, const uint size);
    virtual int PMEntryCompare(const void* e1, const void* e2); // 0 if equl
    virtual void PMEntryCleanup(void* srcPtr, void* dstPtr);
  private: 
    // implementation of virtual HashTable functions on PointerMapEntry
    virtual uint HashFunct(const void* entry, const uint size);
    virtual int EntryCompare(const void* e1, const void* e2); // 0 if equal
    virtual void EntryCleanup(void* entry); 
};


class PointerMapIterator : private HashTableIterator
{
  public:
    PointerMapIterator	(const PointerMap* map);
    ~PointerMapIterator	();

    void*	 CurrentSrc	 () const; // Source pointer of mapped pair
    void*	 CurrentTarget   () const; // Target pointer of mapped pair

    void	 operator++	(int);	// Postfix interator.
    void	 Reset		();	// Restart from beginning.
};

#endif /* support_PointerMap_hpp */
