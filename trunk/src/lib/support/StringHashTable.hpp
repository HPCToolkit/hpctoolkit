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

//***************************************************************************
//
// File:
//    StringHashTable.H
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef StringHashTable_H 
#define StringHashTable_H


//************************* System Include Files ****************************

//*************************** User Include Files ****************************

#include "HashTable.h"
#include "String.h"

//*************************** Forward Declarations ***************************

//****************************************************************************

class StringHashBucket {
public:
  StringHashBucket(const String &_key = String(""), void* _data = NULL)
    : key(_key), data(_data) {
  }

  ~StringHashBucket() {
    // does not own 'data'
  }

  String& GetKey() { return key; }
  void*  GetData() { return data; }

  void SetKey(const String &_key) { key = _key; }
  void SetData(void* _data) { data = _data; }

private:

protected:   
  String key; 
  void* data;  
};

 
uint StringHTHashFunct(const void*, const uint);
uint StringHTRehashFunct(const uint, const uint);
int  StringHTEntryCompare(const void *, const void *);
void StringHTEntryCleanup(void*);


inline String* StringHTBucketExtractKey(const void* entry)
{
  // entry is a StringHashBucket**
  StringHashBucket* bucket = *((StringHashBucket**)entry);
  return &(bucket->GetKey()); // GetKey() returns a String&
}


// Assumes ownership of all StringHashBucket's
 
class StringHashTable : protected HashTable {
public:
  StringHashTable() {
    HashTable::Create(sizeof(void*), 32,
		      StringHTHashFunct,
		      StringHTRehashFunct,
		      StringHTEntryCompare,
		      StringHTEntryCleanup);
  }

  ~StringHashTable();

  // assumes ownership of StringHashBucket's
  void  AddEntry (StringHashBucket * entry) {
    HashTable::AddEntry((void*)&entry);    // takes a pointer to 'entry'
  }
  void  DeleteEntry (StringHashBucket * entry) { 
    HashTable::DeleteEntry((void*)&entry); // takes a pointer to 'entry'
  }
  StringHashBucket* QueryEntry (const void * entry) const {
    StringHashBucket** ret =               // takes a pointer to 'entry'
      (StringHashBucket**)HashTable::QueryEntry((void*)&entry);
    return (ret == NULL) ? NULL : *ret;
  }
  HashTable::NumberOfEntries;

  HashTable::Dump;

  friend class StringHashTableIterator;

protected:

private:  

};


/**************** StringHashTableIterator class definitions *****************/

class StringHashTableIterator : public HashTableIterator
{
public:
  
  StringHashTableIterator(const StringHashTable* theHashTable)
    : HashTableIterator( (const HashTable*)theHashTable ) {
    // Preferable: dynamic_cast<const HashTable*>(theHashTable): GCC
    // 3.2 doesn't like this, saying 'HashTable' is not visible.
    // However, I believe this is wrong: this class is a friend of
    // StringHashTable.  Further, "A base class is said to be
    // accessible if an invented public member of the base class is
    // accessible" (11.2.4 C++ standard), and the compiler (correctly)
    // allows the following (non-invented) member access:
    //   theHashTable->HashTable::AddEntry((void*)NULL);
  }
  
  ~StringHashTableIterator() {
  }

  HashTableIterator::operator ++;
  
  StringHashBucket *Current() const {
    StringHashBucket** ret = (StringHashBucket**)HashTableIterator::Current();
    return (ret == NULL) ? NULL : *ret;
  }
  
  HashTableIterator::Reset;
  
protected:
  
private:
};

 

#endif 
