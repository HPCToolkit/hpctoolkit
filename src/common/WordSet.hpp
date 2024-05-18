// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

//***************************************************************************
//
// WordSet.h
//
// Author:  John Mellor-Crummey                             January 1994
//
//**************************************************************************/

#ifndef WordSet_h
#define WordSet_h

//************************* System Include Files ****************************

#include <iostream>

//*************************** User Include Files ****************************

#include "HashTable.hpp"

//***************************************************************************

//-------------------------------------------------------------
// class WordSet
//-------------------------------------------------------------
class WordSet : private HashTable {
public:
  WordSet();
  WordSet(const WordSet &rhs);
  virtual ~WordSet();

  void Add(unsigned long entry);
  void Delete(unsigned long entry);
  int IsMember(unsigned long entry) const;
  bool Intersects(const WordSet& rhs) const;
  void Clear();

  unsigned long GetEntryByIndex(unsigned int indx) const;

  int operator==(const WordSet &rhs) const;  // equality
  WordSet& operator|=(const WordSet &rhs); // union
  WordSet& operator&=(const WordSet &rhs); // intersection
  WordSet& operator-=(const WordSet &rhs); // difference
  WordSet& operator=(const WordSet &rhs);  // copy

  using HashTable::NumberOfEntries;

  void Dump(std::ostream& file = std::cerr,
            const char* name = "",
            const char* indent = "");

private:
  //-------------------------------------------------------------
  // virtual functions for hashing and comparing
  // that override the defaults for HashTable
  //-------------------------------------------------------------
  unsigned int HashFunct(const void *entry, const unsigned int size);
  int EntryCompare(const void *entry1, const void *entry2); // 0 if equal

//-------------------------------------------------------------
// friend declaration required so HashTableIterator can be
// used with the private base class
//-------------------------------------------------------------
friend class WordSetIterator;
};


//-------------------------------------------------------------
// class WordSetIterator
//-------------------------------------------------------------
class WordSetIterator
  : private HashTableIterator {
public:
  WordSetIterator(const WordSet *theTable);
  virtual ~WordSetIterator() { }
  unsigned long *Current() const;
  using HashTableIterator::operator++;
  using HashTableIterator::Reset;
};

//-------------------------------------------------------------
// class WordSetSortedIterator
//-------------------------------------------------------------
class WordSetSortedIterator : private HashTableSortedIterator {
public:
  WordSetSortedIterator(WordSet const *theTable,
                        EntryCompareFunctPtr const EntryCompare);
  virtual ~WordSetSortedIterator() { }
  unsigned long *Current() const;
  using HashTableSortedIterator::operator++;
  using HashTableSortedIterator::Reset;

private:
  unsigned long current;
};


#endif /* WordSet_h */
