// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

//***************************************************************************
//
// WordSet:
//
//   This table can be used to store a set of words
//
// WordSetIterator:
//
//   for enumerating entries in a WordSet
//
// Author:  John Mellor-Crummey                       January 1994
//
//***************************************************************************

//************************** System Include Files ***************************

//*************************** User Include Files ****************************

#include "WordSet.hpp"

//*************************** Forward Declarations **************************

//***************************************************************************

//**********************************************************************
// implementation of class WordSet
//**********************************************************************


WordSet::WordSet()
{
  HashTable::Create(sizeof(unsigned long), 8);
}


WordSet::WordSet(const WordSet &rhs)
{
  HashTable::Create(sizeof(unsigned long), 8);
  *this |= rhs;
}


WordSet::~WordSet()
{
  HashTable::Destroy();
}


unsigned int WordSet::HashFunct(const void *entry, const unsigned int size)
{
  return *((unsigned long *) entry) % size;
}


int WordSet::EntryCompare(const void *e1, const void *e2)
{
  return  *((unsigned long *) e1) - *((unsigned long *) e2);
}


void WordSet::Add(unsigned long entry)
{
  HashTable::AddEntry(&entry);
}


void WordSet::Delete(unsigned long entry)
{
  HashTable::DeleteEntry(&entry);
}


int WordSet::IsMember(unsigned long entry) const
{
  void *found  = HashTable::QueryEntry(&entry);
  return (found != 0);
}


bool WordSet::Intersects(const WordSet &rhs) const
{
  // identify the larger and smaller of the two set operands
  const WordSet *larger, *smaller;
  if (NumberOfEntries() > rhs.NumberOfEntries()) {
    larger = this; smaller = &rhs;
  } else {
    larger = &rhs; smaller = this;
  }

  // check for intersection by looking up
  // each element of the smaller set in the larger set.
  unsigned long *word;
  for (WordSetIterator words(smaller); (word = words.Current()); words++) {
    if (larger->IsMember(*word)) return true;
  }
  return false;
}


void WordSet::Clear()
{
  if (NumberOfEntries() > 0) {
    HashTable::Destroy();
    HashTable::Create(sizeof(unsigned long), 8);
  }
}


unsigned long WordSet::GetEntryByIndex(unsigned int indx) const
{
  return *((unsigned long *) HashTable::GetEntryByIndex(indx));
}


int WordSet::operator==(const WordSet &rhs) const
{
  //-----------------------------------------
  // false if sets have different cardinality
  //-----------------------------------------
  if (rhs.NumberOfEntries() != NumberOfEntries()) return 0;

  //-----------------------------------------
  // false if some word in rhs is not in lhs
  //-----------------------------------------
  WordSetIterator words(&rhs);
  unsigned long *word;
  for (; (word = words.Current()); words++) if (IsMember(*word) == 0) return 0;

  return 1; // equal otherwise
}


WordSet& WordSet::operator|=(const WordSet &rhs)
{
  unsigned int nentries = rhs.NumberOfEntries();
  for (unsigned int i = 0; i < nentries; i++) {
    Add(rhs.GetEntryByIndex(i));
  }
  return *this;
}

WordSet& WordSet::operator=(const WordSet &rhs)
{
  if (this != &rhs) {
    Clear();
    *this |= rhs;
  }
  return *this;
}


WordSet& WordSet::operator&=(const WordSet &rhs)
{
  // identify the larger and smaller of the two set operands
  const WordSet *larger, *smaller;
  if (NumberOfEntries() > rhs.NumberOfEntries()) {
    larger = this; smaller = &rhs;
  } else {
    larger = &rhs; smaller = this;
  }

  // perform the intersection by looking up
  // each element of the smaller set in the larger set.
  // accumulate the intersection in a temporary set "temp"
  WordSetIterator words(smaller);
  unsigned long *word;
  WordSet temp;
  for (; (word = words.Current()); words++) {
    if (larger->IsMember(*word)) temp.Add(*word);
  }

  // overwrite the current set with the intersection
  return (*this = temp);
}


WordSet& WordSet::operator-=(const WordSet &rhs)
{
  // perform the difference by looking up
  // each element of this set in the rhs set.
  // Delete the members that match
  unsigned long *word;
  for (WordSetIterator words(this); (word = words.Current()); words++) {
    if (rhs.IsMember(*word)) {
      Delete(*word);
    }
  }
  return *this;
}


void
WordSet::Dump(std::ostream& out, const char* name, const char* indent)
{
  out << indent << "WordSet " << name << " " << this << std::endl;
  int countThisLine = 0;
  for (WordSetIterator step(this); step.Current(); step++) {
    if (countThisLine == 0) {
      out << indent << "    ";
    }
    out << *(long *) step.Current() << std::endl;
    if (++countThisLine == 10) {
      out << std::endl;
      countThisLine = 0;
    }
  }
}

//**********************************************************************
// implementation of class WordSetIterator
//**********************************************************************

WordSetIterator::WordSetIterator(const WordSet *theTable)
: HashTableIterator((const HashTable *) theTable)
{
}


unsigned long *WordSetIterator::Current() const
{
  return (unsigned long *) HashTableIterator::Current();
}

WordSetSortedIterator::WordSetSortedIterator (const WordSet *theTable,
                                              EntryCompareFunctPtr const WSEntryCompare)
  : HashTableSortedIterator((const HashTable *) theTable,
                            WSEntryCompare), current(0)
{
}

unsigned long *WordSetSortedIterator::Current() const
{
  if (HashTableSortedIterator::IsValid()) {
    ((WordSetSortedIterator *)this)->current =
      *(unsigned long*) HashTableSortedIterator::Current();
    return &((WordSetSortedIterator *)this)->current;
  } else return 0;
}
