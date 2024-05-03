// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

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
