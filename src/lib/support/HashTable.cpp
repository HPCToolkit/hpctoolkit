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
 * File:    C++ Hash Table Utility                                            *
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

#ifdef NO_STD_CHEADERS
# include <stdlib.h>
# include <string.h>
#else
# include <cstdlib>
# include <cstring>
using namespace std; // For compatibility with non-std C headers
#endif

//*************************** User Include Files ****************************

#include <include/gcc-attr.h>

#include "HashTable.hpp"

#include <lib/support/diagnostics.h>

/******************************* local defines *******************************/

using std::cout;
using std::endl;

static const int FIRST_SLOT                 =   0;
static const int EMPTY                      =  -1;
static const int DELETED                    =  -2;
static const int INVALID_INDEX              = -99;

static const int ENTRY_DEPTH_FOR_HASHING    =  32;
static const int LOOKING_FOR_AN_INDEX       =   1;

static ulong NEXT_ID = 0;

/******************* HashTable static function prototypes ********************/

static uint DefaultHashFunct  (const void* entry, const uint size);
static uint DefaultRehashFunct  (const uint oldHashValue, const uint size);
static int DefaultEntryCompare  (const void* entry1, const void* entry2);
static void DefaultEntryCleanup  (void* entry);

/********************* HashTable public member functions *********************/

//
//
HashTable::HashTable ()
  : id(NEXT_ID++)
{
  numSlots     = 0;
  nextSlot     = FIRST_SLOT;
  entrySize    = 0;
  entries      = (void*)NULL;
  indexSetSize = 0;
  indexSet     = (int*)NULL ;

  hashTableCreated = false;

  HashFunctCallback    = (HashFunctFunctPtr)DefaultHashFunct;
  RehashFunctCallback  = (RehashFunctFunctPtr)DefaultRehashFunct;
  EntryCompareCallback = (EntryCompareFunctPtr)DefaultEntryCompare;
  EntryCleanupCallback = (EntryCleanupFunctPtr)DefaultEntryCleanup;

  return;
}

//
//
HashTable::~HashTable ()
{
  if (hashTableCreated)
    {
       FailureToDestroyError();
    }
}

//
//
void HashTable::Create (const uint theEntrySize, uint initialSlots,
                        HashFunctFunctPtr    const _HashFunctCallback,
                        RehashFunctFunctPtr  const _RehashFunctCallback,
                        EntryCompareFunctPtr const _EntryCompareCallback,
                        EntryCleanupFunctPtr const _EntryCleanupCallback)
{

   if (!hashTableCreated)
     {
       register unsigned int power = 0, i = 31;
  
       if (initialSlots < 8) initialSlots = 8;
   
       while (i > 1)   // compute size for the sparse index set
         {
           if (initialSlots & (1 << i))
             {
               if (i < 20) power = (1 << (i + 2)) - 1;
               else        power = (1 << i) - 1;
               break;
             }
           i--;
         }
  
       numSlots     = initialSlots;
       nextSlot     = FIRST_SLOT;
       entrySize    = theEntrySize;
       entries      = (void*) new char[entrySize * numSlots];
       indexSetSize = power;
       indexSet     = new int[indexSetSize];
  
       memset (entries, 0, entrySize * numSlots);

       for (i = 0; i < indexSetSize; i++) indexSet[i] = EMPTY;

          //  Set the function pointer and the HashTable will use those
          //  instead of the virtual functions.  If any are left out (or
          //  set NULL by the user) just use the virtual function as a 
          //  default.
       if (_HashFunctCallback)    HashFunctCallback = _HashFunctCallback;

       if (_RehashFunctCallback)  RehashFunctCallback = _RehashFunctCallback;

       if (_EntryCompareCallback) EntryCompareCallback = _EntryCompareCallback;

       if (_EntryCleanupCallback) EntryCleanupCallback = _EntryCleanupCallback;

#      ifdef DEBUG
           cerr << "\tHashTable::HashTable" << "\n"
                << "\t\tnumSlots: " << numSlots << "\n"
                << "\t\tnextSlot: " << nextSlot << "\n"
                << "\t\tentrySize: " << entrySize << "\n"
                << "\t\tentries: " <<  entries << "\n"
                << "\t\tindexSetSize: " << indexSetSize << "\n"
                << "\t\tindexSet: " << indexSet << "\n" 
                << endl;
#      endif

       hashTableCreated = true;
     }

   return;
}

//
//
void HashTable::Destroy ()
{
   if (hashTableCreated)
     {
       char* cEntries = (char*)entries;
       for (unsigned int entryIndex = 0; entryIndex < nextSlot; entryIndex++)
         {
           EntryCleanup(cEntries);
	   cEntries += entrySize;
         }
       delete [] (char*)entries;
       delete [] (char*)indexSet;

       numSlots     = 0;
       nextSlot     = FIRST_SLOT;
       entrySize    = 0;
       entries      = NULL;
       indexSetSize = 0;
       indexSet     = NULL;
       hashTableCreated = false;

       HashFunctCallback    = (HashFunctFunctPtr)DefaultHashFunct;
       RehashFunctCallback  = (RehashFunctFunctPtr)DefaultRehashFunct;
       EntryCompareCallback = (EntryCompareFunctPtr)DefaultEntryCompare;
       EntryCleanupCallback = (EntryCleanupFunctPtr)DefaultEntryCleanup;
     }
}

//
// Explicitly defined to prevent usage. 
HashTable &HashTable::operator=(const HashTable& GCC_ATTR_UNUSED rhs)
{
  DIAG_Die("Should not call HashTable::operator=()!");
  return *this;
}

//
//
bool HashTable::operator==(HashTable& rhsTab)
{
  return (id == rhsTab.id);
}

//
//
void HashTable::AddEntry (void* entry, AddEntryFunctPtr const AddEntryCallback, ...)
{
  if (hashTableCreated)
    {
       va_list argList;
       int   index = QueryIndexSet (entry, true);
       char* cEntries = (char*)entries;
  
       DIAG_Assert(index != INVALID_INDEX, "");

#      ifdef DEBUG
         cerr << "HashTable::AddEntry: using index " << index 
              << endl;
#      endif
  
       if (indexSet[index] < 0)
         { // add entry
            indexSet[index] = nextSlot++;
            memcpy (&cEntries[entrySize * indexSet[index]], entry, entrySize);
            if (nextSlot == numSlots) OverflowEntries ();
         } 
       else
         { // entry already present
            if (AddEntryCallback != NULL)
              { 
                 va_start (argList, AddEntryCallback);
                 AddEntryCallback ((void*)&cEntries[entrySize * indexSet[index]], 
                                   entry, argList);
                 va_end (argList);
              }
            else
              {
                 EntryCleanup ((void*)&cEntries[entrySize * indexSet[index]]);
                 memcpy (&cEntries[entrySize * indexSet[index]], entry, entrySize);
              }
         }
    }
  else
    {
       FailureToCreateError ();
       return;
    }
}

//
//
void HashTable::DeleteEntry (void* entry, DeleteEntryFunctPtr const DeleteEntryCallback, ...)
{
  if (hashTableCreated)
    {
       va_list argList;
       int index = QueryIndexSet (entry, false);
       char* cEntries = (char*) entries;
  
       if (index == INVALID_INDEX || indexSet[index] == EMPTY || indexSet[index] == DELETED)
         { // entry doesn't exist
#           ifdef DEBUG
                cerr << "\tHashTable::DeleteEntry: entry not found." 
                     << endl;
#           endif
         }
       else
         { // move the last entry into the vacant slot and fix the index set
#           ifdef DEBUG
              cerr << "\tHashTable::DeleteEntry: deleting entry for index " << index 
                   << endl;
#           endif

            int last = nextSlot - 1;

#           ifdef DEBUG
              cerr << "\tHashTable::DeleteEntry: moving last entry to empty slot." 
                   << endl;
#           endif

            if (last >= 0)
              {
                if (indexSet[index] != last) 
                  {
                     char* tempEntry = new char[entrySize];
                     int tempIndexSetValue = 0;
                     int toIndex = index;
                     int fromIndex = QueryIndexSet (&cEntries[last * entrySize], false);
 
                     memcpy (tempEntry, 
                             &cEntries[entrySize * indexSet[toIndex]], 
                             entrySize);
                     memcpy (&cEntries[entrySize * indexSet[toIndex]],
                             &cEntries[entrySize * indexSet[fromIndex]], 
                             entrySize);
                     memcpy (&cEntries[entrySize * indexSet[fromIndex]], 
                             tempEntry, 
                             entrySize);
    
                     tempIndexSetValue = indexSet[toIndex];
                     indexSet[toIndex] = indexSet[fromIndex];
                     indexSet[fromIndex] = tempIndexSetValue;

                     delete [] tempEntry;
                  }
    
                nextSlot = last;
                indexSet[index] = DELETED;

                if (DeleteEntryCallback != NULL)
                  {
                    va_start (argList, DeleteEntryCallback);
                    DeleteEntryCallback ((void*)&cEntries[last * entrySize], argList);
                    va_end (argList);
                  }
                else
                  {
                    EntryCleanup ((void*)&cEntries[last * entrySize]);
                  }

                memset (&cEntries[entrySize * last], 0, entrySize); // finish deletion
              }
            else
              { // there is only one entry left in the table
                nextSlot = 0;
                indexSet[index] = DELETED;

                if (DeleteEntryCallback != NULL)
                  {
                    va_start (argList, DeleteEntryCallback);
                    DeleteEntryCallback ((void*)&cEntries[0], argList);
                    va_end (argList);
                  }
                else
                  {
                    EntryCleanup ((void*)&cEntries[0]);
                  }

                memset (&cEntries[0], 0, entrySize); // finish deletion
              }
         }
    }
  else
    {
       FailureToCreateError ();
       return;
    }
}

//
//
void* HashTable::QueryEntry (const void* entry) const
{
  if (hashTableCreated)
    {
       int index = QueryIndexSet (entry, false);
       char* cEntries = (char*) entries;

#      ifdef DEBUG
           cerr << "\tHashTable::QueryEntry (" << entry << ")."
                << endl;
#      endif

       if (index == INVALID_INDEX || indexSet[index] == EMPTY || indexSet[index] == DELETED)
         {
            return (void*)NULL;
         }
       else
         {
            return (void*)(&cEntries[entrySize * indexSet[index]]);
         }
    }
  else
    {
       FailureToCreateError ();
       return (void*)NULL;
    }
}

//
//
int HashTable::GetEntryIndex (const void* entry) const
{
  if (hashTableCreated)
    {
       int index = QueryIndexSet (entry, false);

       if (index == INVALID_INDEX || indexSet[index] < 0) return EMPTY;
       else return indexSet[index];
    }
  else
    {
       FailureToCreateError ();
       return EMPTY;
    }
}

//
//
void* HashTable::GetEntryByIndex (const uint index) const
{
  if (hashTableCreated)
    {
       if (index >= nextSlot) return (void*)NULL;
       else return (void*)&((char*)entries)[entrySize * index];
    }
  else
    {
       FailureToCreateError ();
       return (void*)NULL;
    }
}

//
//
uint HashTable::NumberOfEntries () const
{
  if (hashTableCreated)
    {
       return nextSlot;
    }
  else
    {
       FailureToCreateError ();
       return 0;
    }
}

//
//
void HashTable::Dump ()
{
   cout << "HashTable: " << this << " (id: " << id << ")" << endl; 
   if (hashTableCreated) {
      HashTableIterator stp(this); 
      int i; 

      for (i =0; stp.Current(); stp++, i++) {
         cout << i << " : indx " << GetEntryIndex(stp.Current()) 
		   << " : ptr  " << stp.Current() << endl;
      } 
   } else {
      cout << "     NOT CREATED" << endl; 
   } 
}

/******************* HashTable protected member functions ********************/

//
//
void HashTable::Create (const uint theEntrySize, uint initialSlots)
{
   if (!hashTableCreated)
     {
       register unsigned int power = 0, i = 31;
  
       if (initialSlots < 8) initialSlots = 8;
   
       while (i > 1)   // compute size for the sparse index set
         {
           if (initialSlots & (1 << i))
             {
               if (i < 20) power = (1 << (i + 2)) - 1;
               else        power = (1 << i) - 1;
               break;
             }
           i--;
         }
  
       numSlots     = initialSlots;
       nextSlot     = FIRST_SLOT;
       entrySize    = theEntrySize;
       entries      = (void*) new char[entrySize * numSlots];
       indexSetSize = power;
       indexSet     = new int[indexSetSize];
  
       memset (entries, 0, entrySize * numSlots);

       for (i = 0; i < indexSetSize; i++) indexSet[i] = EMPTY;

#      ifdef DEBUG
           cerr << "\tHashTable::HashTable\n"
                << "\t\tnumSlots: " << numSlots << "\n"
                << "\t\tnextSlot: " << nextSlot << "\n"
                << "\t\tentrySize: " << entrySize << "\n"
                << "\t\tentries: " << entries << "\n"
                << "\t\tindexSetSize: " << indexSetSize << "\n"
                << "\t\tindexSet: " << indexSet << "\n"
                << endl;
#      endif

       hashTableCreated = true;
     }

   return;
}

//
//
uint HashTable::HashFunct (const void* entry, const uint size) 
{
  return HashFunctCallback (entry, size);
}

//
//
uint HashTable::RehashFunct (const uint oldHashValue, const uint size) 
{
  return RehashFunctCallback (oldHashValue, size);
}

//
//
int HashTable::EntryCompare (const void* entry1, const void* entry2)
{
  return EntryCompareCallback (entry1, entry2);
}

//
//
void HashTable::EntryCleanup (void* entry)
{
  EntryCleanupCallback (entry);
}

/********************* HashTable private member functions ********************/

//
//
int HashTable::QueryIndexSet (const void* entry, const bool addingEntry) const
{
  int initialIndex, currentIndex;
  int firstDeletedIndex = INVALID_INDEX;
  int rehashCount = 0; 
  char* cEntries = (char*)entries;
  HashTable* This = (HashTable*)this;  // for circumventing const'ness

  currentIndex = initialIndex = This->HashFunct(entry, indexSetSize);

# ifdef DEBUG
    cerr << "\tHashTable::QueryIndexSet(" << entry << ", " << addingEntry << ")." 
         << endl;
# endif
  
  while (LOOKING_FOR_AN_INDEX)
    {
       switch (indexSet[currentIndex])
         {
            case EMPTY:
              if (firstDeletedIndex != INVALID_INDEX)
                {     // if there was a "DELETED" slot before the "EMPTY" slot, use it.
                   return firstDeletedIndex;
                }
              else
                {     // otherwise, use the "EMPTY" slot.
                   return currentIndex;
                }

            case DELETED:
              if (firstDeletedIndex != INVALID_INDEX)
                {
                   if (firstDeletedIndex == currentIndex) 
                     {  // wrapped the table, use this first "DELETED" slot.
                        return currentIndex;
                     }
                }
              else 
                {     // tag the first encountered "DELETED" slot for the future.
                   firstDeletedIndex = currentIndex;
                }
            break;

            default:
              if (This->EntryCompare(entry, 
                                        &cEntries[entrySize * indexSet[currentIndex]]) == 0)
                {
                   return currentIndex;
                }
            break;
         }
   
#      ifdef DEBUG
         cerr << "\tRehashing the Index" 
              << endl;
#      endif

       currentIndex = This->RehashFunct(currentIndex, indexSetSize);
       rehashCount ++;

       if (addingEntry)
         {     // if we've rehashed to much and the table is more than half full or
               // we've wrapped the table looking for an empty slot, then
               // increase the size of the table and rehash the entries fix problem.
            if (((rehashCount > 10) && ((2 * nextSlot) > indexSetSize)) || 
                (currentIndex == initialIndex))
              {
                 This->OverflowIndexSet ();
                 return QueryIndexSet (entry, false);
              }
         }
       else if (currentIndex == initialIndex)
         {     // not adding an entry and we've wrapped table looking for the entry.
               // conclusion is it's not here, so let's leave.
            return INVALID_INDEX;
         }
    }
}

//////////////////////////////////////////////////////////////////////////////
//
//  The indexSet table can overflow for two very different reasons:
//
//  (1)	It is full.  This only happens with a reasonably good distribution
//	from the Hash function.  (Check the collision limit code in
//	HashTableIndexSet().  If we get here with a full table, we should add
//	a reasonable increment -- after all, the indexSet table is designed
//	to be a sparse table.  (See the code in HashTable() that picks the
//	indexSet set size as a function of the number of requested slots.)
//
//  (2) We're getting bad behavior from the hash function and the table
//	is moderately full.  (The combination of a table that is over
//	50% full and an insertion that has more than ten probes triggers
//	an overflow.)  In this case, we'd like to change the distribution.
//	Typically, two factors make a difference here: the density of
//	entries in the indexSet table and numerical properties involving
//	the MOD oepration in the hash function.
//
//   So, after much discussion and some limited experimentation, we chose
//   the following scheme.  The indexSet set size is enlarged by roughly the
//   number of symbols already entered (nextSlot).
//
//   I say "roughly", because we first mask off the low order bit to ensure
//   that the increment is even.  This keeps the rehash quotient relatively
//   prime to the size of the indexSet table.  It also ensures that the MOD
//   operation in the hash function is taken relative to an odd number.
//   This keeps it from being a simple masking off of some high order bits.
//   (We hope that this changes the distribution radically in pathological
//   cases.  Our thinking is that too many collisions should trigger a
//   resize on the indexSet set.  In turn, that should change the distribution
//   enough to get us out of the collisions.
//
//   Linda Torczon suggested the rehash limit.  The decisions about
//   incremental size increases for both indexSet and Entries sets were the
//   result of a lengthy discussion.  We hope that this scheme avoids
//   most pathology and provides reasonable behavior.
//
//////////////////////////////////////////////////////////////////////////////

//
//
void HashTable::OverflowIndexSet()
{
  register unsigned int i, initialIndex, finalIndex, size;
  char* cEntries = (char*)entries;
  
  if (indexSetSize < 4096)
    {                                   // big table => more careful choice
       size = indexSetSize + 1024;      // small table => big increment
    }                                   // big table => more careful choice
  else 
    {                                   // big table => more careful choice
       size  = nextSlot & 0xFFFFFFFE;   // make it even
       size += indexSetSize;	        // add number of entries to index set
    }				        
  
# ifdef DEBUG
    cerr << "\tHashTable::OverflowIndexSet: old size: " << indexSetSize 
         << ", new size: " << size
         << endl;
# endif
  
  indexSetSize = size;
  
  if (indexSet != 0) delete [] indexSet;
  
  indexSet = new int[indexSetSize];
  
  for (i = 0; i < indexSetSize; i++) indexSet[i] = EMPTY;
  
  for (i = 0; i < nextSlot; i++)
    {
       finalIndex = initialIndex = HashFunct (&cEntries[entrySize * i], indexSetSize);
    
       while(indexSet[finalIndex] != EMPTY)
         {
#           ifdef DEBUG
              cerr << "\tRehashing for OverflowIndexSet()" 
                   << endl;
#           endif

            finalIndex = RehashFunct (finalIndex, indexSetSize);
            DIAG_Assert(finalIndex != initialIndex, "shouldn't wrap table w/o allocating an index");
         }

       indexSet[finalIndex] = i;
    }

  return;
}

//
//
void HashTable::OverflowEntries ()
{
  int newSlots, newSize, oldSize;
  char* ptr;
  
  if (numSlots < 4096) newSlots = numSlots + 512;
  else if (numSlots < 16384) newSlots = numSlots + 2048;
  else newSlots = (int) (1.33 * numSlots);
  
  oldSize  = numSlots * entrySize; 
  newSize  = newSlots * entrySize;
  
  ptr = new char [newSize];
  memset (ptr, 0, newSize);
  memcpy (ptr, entries, oldSize);  // copy the old values
  
  delete [] (char*)entries;                  // and replace it
  entries = ptr;
  
# ifdef DEBUG
    cerr << "\tHashTable::OverflowEntries: old slots: " << numSlots
         << ", new slots: " << newSlots 
         << endl;
# endif
  
  numSlots = newSlots;

  return;
}

//
//
void HashTable::FailureToCreateError () const
{
  DIAG_Die("Failure to call Create() before using the HashTable");
}

//
//
void HashTable::FailureToDestroyError () const
{
  DIAG_Die("Failure to call Destroy() before deleting the HashTable");
}

/************************ HashTable extern functions *************************/

//
//
uint IntegerHashFunct (const int value, const uint size)
{
   return (uint)((int)value % size);
}

//
//
uint IntegerRehashHashFunct (const uint oldHashValue, const uint size)
{
      // 16 is relatively prime to a Mersenne prime!
  return (uint)((oldHashValue + 16) % size); 
}

//
//
int IntegerEntryCompare (const int value1, const int value2)
{
   return (int)((int)value1 != (int)value2);
}

//
//
uint StringHashFunct (const void* entry, const uint size)
{
  uint  result = 0;
  char* cEntry = (char*) entry;
  unsigned char c;

  for (int i = 0; (i < ENTRY_DEPTH_FOR_HASHING) && (*cEntry != '\000'); i++)
    {
       c = (unsigned char) *cEntry++;
       result += (c & 0x3f);
       result = (result << 5) + (result >> 27);
    }

  return (result % size);
}

//
//
uint StringRehashFunct (const uint oldHashValue, const uint size)
{
      // 16 is relatively prime to a Mersenne prime!
  return (uint)((oldHashValue + 16) % size); 
}

//
//
int StringEntryCompare (const void* entry1, const void* entry2)
{
  char* cEntry1 = (char*)entry1;
  char* cEntry2 = (char*)entry2;

  return strcmp (cEntry1, cEntry2);
}

/************************ HashTable static functions *************************/

//
//
static uint DefaultHashFunct (const void* GCC_ATTR_UNUSED entry,
			      const uint GCC_ATTR_UNUSED size)
{
  DIAG_Die("Failure to specify HashFunct function.");
  return 0;
}

//
//
static uint DefaultRehashFunct (const uint oldHashValue, const uint size)
{
  uint newHashValue;

      // 16 is relatively prime to a Mersenne prime!
  newHashValue = (oldHashValue + 16) % size; 

# ifdef DEBUG
      cerr << "\tHashTable::DefaultRehashFunct\t" << oldHashValue 
           << " ---> " << newHashValue 
           << endl; 
# endif

  return newHashValue;
}

//
//
static int DefaultEntryCompare (const void* GCC_ATTR_UNUSED entry1,
				const void* GCC_ATTR_UNUSED entry2)
{
  DIAG_Die("Failure to specify EntryCompare function.");
  return 0;
}

//
//
static void DefaultEntryCleanup (void* GCC_ATTR_UNUSED entry)
{
# ifdef DEBUG
    cerr << "\tHashTable::DefaultEntryCleanup\t" << (int)entry
         << endl;
# endif

  return;
}

/****************** HashTableIterator public member functions ****************/

//
//
HashTableIterator::HashTableIterator (const HashTable* theHashTable)
{
  hashTable = (HashTable*)theHashTable;

  HashTableIterator::Reset();

  return;
}

//
//
HashTableIterator::~HashTableIterator ()
{
  return;
}

//
//
void HashTableIterator::operator ++(int)
{
  currentEntryNumber--;

  return;
}

//
//
void* HashTableIterator::Current () const
{
  // Second condition could still arise if someone tries to access the
  // current entry after calling DeleteEntry on it.
  
  if (currentEntryNumber >= 0 &&
      (unsigned int)currentEntryNumber < ((HashTable*)hashTable)->nextSlot)
    {
       char* cEntries = (char*)((HashTable*)hashTable)->entries;

       return (void*)&cEntries[currentEntryNumber * ((HashTable*)hashTable)->entrySize];
    }
  else
    {
       return (void*)NULL;
    }
}

//
//
void HashTableIterator::Reset ()
{
  currentEntryNumber = ((HashTable*)hashTable)->nextSlot - 1;

  return;
}
