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


