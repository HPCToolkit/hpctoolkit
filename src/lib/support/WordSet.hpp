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
  uint HashFunct(const void *entry, const uint size);
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
