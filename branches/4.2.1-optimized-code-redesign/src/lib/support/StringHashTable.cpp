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
//    StringHashTable.C
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

//************************* System Include Files ****************************

#include <vector> // STL

//*************************** User Include Files ****************************

#include <include/general.h>

#include "StringHashTable.hpp"

//*************************** Forward Declarations ***************************

//****************************************************************************

uint StringHTHashFunct(const void* entry, const uint size)
{
  String* str = StringHTBucketExtractKey(entry);
  const char* key = (const char*) *str;
  return StringHashFunct(key, size);
}

uint StringHTRehashFunct (const uint oldHashValue, const uint size)
{
  return StringRehashFunct(oldHashValue, size);
}

// Return 1 if key1 == key2; 0 otherwise
int StringHTEntryCompare(const void* entry1, const void* entry2)
{
  String* str1 = StringHTBucketExtractKey(entry1);
  String* str2 = StringHTBucketExtractKey(entry2);
  const char* key1 = (const char*)*str1, *key2 = (const char*)*str2;
  return StringEntryCompare(key1, key2);
}

void StringHTEntryCleanup(void* entry)
{
  return; 
}


StringHashTable::~StringHashTable()
{
  // delete all StringHashBucket's because we own them -- but we can't
  // delete the buckets without first erasing the position the bucket
  // is in because each hashtable positon must have a valid key (which
  // comes from the StringHashBucket).  I've chosen to collect all the
  // buckets in a vector.

  std::vector<StringHashBucket*> tmpV(NumberOfEntries());
  unsigned int i = 0;
  for (StringHashTableIterator it(this); it.Current() != NULL; it++, i++) {
    tmpV[i] = it.Current();
  }
  
  for (i = 0; i < tmpV.size(); i++) {
    delete tmpV[i];  tmpV[i] = NULL;
  }
  
  HashTable::Destroy();
}


