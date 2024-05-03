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
 * See the include file HashTable.h for an explanation of the                 *
 * data and functions in the HashTable class.                                 *
 *                                                                            *
 *                    Programming Environment Project                         *
 *                                                                            *
 *****************************************************************************/

//************************** System Include Files ***************************

#include <iostream>

//*************************** User Include Files ****************************

//*************************** Forward Declarations **************************

//***************************************************************************

#include "HashTable.hpp"
#include "QuickSort.hpp"

/*************** HashTableSortedIterator public member functions *************/

//
//
HashTableSortedIterator::HashTableSortedIterator(const HashTable* theHashTable,
                                                 EntryCompareFunctPtr const _EntryCompare)
{
  sortedEntries = (void **) NULL;
  hashTable = (HashTable*) theHashTable;
  EntryCompare = _EntryCompare;

  HashTableSortedIterator::Reset();

  return;
}

//
//
HashTableSortedIterator::~HashTableSortedIterator ()
{
  if (sortedEntries) delete [] sortedEntries;

  return;
}

//
//
void HashTableSortedIterator::operator ++(int)
{
  currentEntryNumber++;
  return;
}

bool HashTableSortedIterator::IsValid() const
{
  return (currentEntryNumber < numberOfSortedEntries);
}

//
//
void* HashTableSortedIterator::Current () const
{
  if (currentEntryNumber < numberOfSortedEntries)
    {
      if (sortedEntries) return (void*)sortedEntries[currentEntryNumber];
      else               return (void*)NULL;
    }
  else
    {
      return (void*)NULL;
    }
}

//
//
void HashTableSortedIterator::Reset ()
{
  HashTableIterator  anIterator(hashTable);
  QuickSort          localQuickSort;

  currentEntryNumber = 0;

  if (sortedEntries) delete [] sortedEntries;

  numberOfSortedEntries = hashTable->NumberOfEntries();

  sortedEntries = new  void* [numberOfSortedEntries];

  for (int i = 0; i < numberOfSortedEntries; i++, anIterator++)
    {
       sortedEntries[i] = anIterator.Current();
    }

  localQuickSort.Create(sortedEntries, EntryCompare);
  localQuickSort.Sort(0, numberOfSortedEntries-1);
  localQuickSort.Destroy();

  return;
}
