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

/******************************************************************************
 *                                                                            *
 * File:    C++ Hash Table Sorted Iterator Utility                            *
 * Author:  Kevin Cureton                                                     *
 * Date:    March 1993                                                        *
 *                                                                            *
 *************************** HashTableSortedIterator **************************
 *                                                                            *
 * HashTableSortedIterator Class                                              *
 *   Used to step through a sorted list of the entries in the HashTable.      *
 *                                                                            *
 * Constructor:                                                               *
 *   Takes a pointer to the HashTable it is desired to iterate over and a     *
 *   pointer to a function which is used to sort the entries.                 *
 *                                                                            *
 * Destructor:                                                                *
 *   Nothing.                                                                 *
 *                                                                            *
 * operator ++:                                                               *
 *   Advances the iterator to the next sorted entry in the HashTable.         *
 *                                                                            *
 * Current:                                                                   *
 *   Returns a pointer to the current sorted entry in the HashTable pointed   *
 *   to by the iterator.                                                      *
 *                                                                            *
 * Reset:                                                                     *
 *   Resets the iterator to point back to the first sorted entry in the       *
 *   HashTable.                                                               *
 *                                                                            *
 ******************************************************************************
 *                                                                            *
 *                    Programming Environment Project                         *
 *                                                                            *
 *****************************************************************************/

#ifndef support_HashTableSortedIterator_hpp
#define support_HashTableSortedIterator_hpp


//************************** System Include Files ***************************

//*************************** User Include Files ****************************

#include "HashTable.hpp"

//*************************** Forward Declarations **************************

//***************************************************************************

/***************** HashTableSortedIterator class definitions *****************/

class HashTableSortedIterator
{
public:
  HashTableSortedIterator(const HashTable* theHashTable,
			  EntryCompareFunctPtr const _EntryCompare);
  virtual ~HashTableSortedIterator();
  
  void  operator ++(int);
  bool IsValid() const;
  void* Current() const;
  void  Reset();
  
private:
  int    currentEntryNumber;
  int    numberOfSortedEntries;
  void** sortedEntries;
  
  const HashTable* hashTable;
  
  EntryCompareFunctPtr EntryCompare;

};

#endif // support_HashTableSortedIterator_hpp

