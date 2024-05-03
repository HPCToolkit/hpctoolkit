// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

/******************************************************************************
 *                                                                            *
 * File:    C++ Hash Table Utility                                            *
 * Author:  Kevin Cureton                                                     *
 * Date:    March 1993                                                        *
 *                                                                            *
 *************************** HashTable Usage Warning **************************
 *                                                                            *
 * Since the HashTable uses callbacks and virtual functions, use care if any  *
 * function that is overridden calls the functions of the HashTable.  There   *
 * is the possibility of recursive function calls if this is the case.         *
 * The functions are designed such that recursive calls can take place, but   *
 * if the overriding functions are not designed carefully, poor code          *
 * performance can result.                                                    *
 *                                                                            *
 ******************************* HashTable Public *****************************
 *                                                                            *
 * Constructor:                                                               *
 *   Allocate and initialize a new hash table (setting a unique id).          *
 *                                                                            *
 * Destructor:                                                                *
 *   Deallocates all storage for the hash table.  Returns an error if the     *
 *   user failed to call Destroy before deleting the table.                   *
 *                                                                            *
 * Create:                                                                    *
 *   Must be called before the HashTable can be used.                         *
 *                                                                            *
 *   entrySize    - size in bytes of each entry in the hash table.            *
 *   initialSlots - the initial number of entries for the hash table.         *
 *                  table.  The table will expand automatically as needed.    *
 *   Functions    - pointers to functions the user wants to use to override   *
 *                  the default behavior of the HashTable.  If any value      *
 *                  is null, then the default function will be used.          *
 *                                                                            *
 *                  Basic Hash, Rehash, and Compare functions have been       *
 *                  supplied for integers and strings.  The external          *
 *                  prototypes are supplied below.                            *
 *                                                                            *
 *   HashFunctCallback    - user-defined hash function.                       *
 *     entry - void* to the entry in the HashTable.                           *
 *     size  - the current max size of the HashTable.                         *
 *   RehashFunctCallback  - user-defined rehash function.                     *
 *     oldHashValue - the previous hash value.                                *
 *     size  - the current max size of the HashTable.                         *
 *   EntryCompareCallback - user-defined compare function.                    *
 *     entry - void* to the first entry.                                      *
 *     entry - void* to the second entry.                                     *
 *   EntryCleanupCallback - user-defined cleanup function.                    *
 *     entry - void* to the entry in the HashTable.                           *
 *                                                                            *
 *   Returns nothing.                                                         *
 *                                                                            *
 * Destroy:                                                                   *
 *   Calls DeleteEntry for all the entries in the table.                      *
 *                                                                            *
 *   Returns nothing.                                                         *
 *                                                                            *
 * operator==:                                                                *
 *   Tests equality on a unique id. Returns true/false.                       *
 *                                                                            *
 * AddEntry:                                                                  *
 *   Add an entry to the hash table.  If two entries hash to the same value,  *
 *   they are compared using the EntryCompare function.                       *
 *                                                                            *
 *   entry            - pointer to the entry to be hashed.                    *
 *   AddEntryCallback - user supplied function which is executed if an        *
 *                      identical entry is found while adding the current     *
 *                      entry.  If a function is not supplied, the default    *
 *                      action is to run EntryCleanup on the old entry and    *
 *                      copy the new entry onto the old entry's location.     *
 *   ...              - arguments which are passed to the AddEntryCallback    *
 *                      when it is executed.  An example of how to use        *
 *                      variable arguments is provided below.                 *
 *                                                                            *
 *   void AddEntryCallback (void* entryyCur, void *entryNew, va_list args)    *
 *   {                                                                        *
 *     while (STILL_MORE_ARGUMENTS_TO_GET)                                    *
 *       {                                                                    *
 *          theArgValue = va_arg (argList, argType);                          *
 *       }                                                                    *
 *   }                                                                        *
 *                                                                            *
 *   Returns nothing.                                                         *
 *                                                                            *
 * DeleteEntry:                                                               *
 *   Delete an entry from the hash table.  Before the entry is deleted,       *
 *   the DeleteEntryCallback is called to manage the cleanup of the entry.    *
 *                                                                            *
 *   entry               - pointer to the entry to be deleted.                *
 *   DeleteEntryCallback - user supplied function which is executed before    *
 *                         deleting the entry.  If this function is not       *
 *                         supplied, the EntryCleanup is called as the        *
 *                         default.                                           *
 *   ...                 - arguments which are passed to the                  *
 *                         DeleteEntryCallback function when it is executed.  *
 *                         An example of how to use variable arguments        *
 *                         is provided below.                                 *
 *                                                                            *
 *   void UserDeleteEntry (void *entryCur, void *entryNew, va_list args)      *
 *   {                                                                        *
 *      while (STILL_MORE_ARGUMENTS_TO_GET)                                   *
 *        {                                                                   *
 *           theArgValue = va_arg (argList, argType);                         *
 *        }                                                                   *
 *   }                                                                        *
 *                                                                            *
 *   Returns nothing.                                                         *
 *                                                                            *
 * QueryEntry:                                                                *
 *   Query the hash table to determine if an entry is present.                *
 *                                                                            *
 *   entry - pointer to the entry to be queried.                              *
 *                                                                            *
 *   Returns a pointer to the entry which was queried or NULL if the entry    *
 *   is not in the hash table.                                                *
 *                                                                            *
 * GetEntryIndex:                                                             *
 *   Get the index of an entry in the hash table.                             *
 *                                                                            *
 *   entry - pointer to the entry whose index is wanted.                      *
 *                                                                            *
 *   Returns the index of the entry.                                          *
 *                                                                            *
 * GetEntryByIndex:                                                           *
 *   Get a pointer to an entry based on an index.                             *
 *                                                                            *
 *   index - the index returned from a call to GetEntryIndex.  Used to access *
 *           the entry.                                                       *
 *                                                                            *
 *   Returns a pointer to the entry.                                          *
 *                                                                            *
 * NumberOfEntries:                                                           *
 *   Get the number of entries in the hash table.                             *
 *                                                                            *
 *   Returns number of entries in the hash table.                             *
 *                                                                            *
 * Dump:                                                                      *
 *   Print the table to cout                                                  *
 *                                                                            *
 ****************************** HashTable Protected ***************************
 *                                                                            *
 * If it is desired, a derived class can be created and these four functions  *
 * can be overridden.  This allows for more specialized operation of the      *
 * HashTable.  If a derived class is created, use the Create function         *
 * supplied in the protected section.  This will ensure that the virtual      *
 * functions are called correctly.                                            *
 *                                                                            *
 * Create:                                                                    *
 *   Must be called before the HashTable can be used.                         *
 *                                                                            *
 *   entrySize    - size in bytes of each entry in the hash table.            *
 *   initialSlots - the initial number of entries for the hash table.         *
 *                  table.  The table will expand automatically as needed.    *
 *                                                                            *
 * The functions perform the same operation as the functions described for    *
 * creating the HashTable with function pointers.  If no virtual function is  *
 * supplied, the default function will be used.                               *
 *                                                                            *
 *   virtual HashFunct:                                                       *
 *      This function must be supplied by the derived class.  Failure to do   *
 *      so will result in an assert(0) being called.                          *
 *                                                                            *
 *   virtual RehashFunct:                                                     *
 *      A basic rehash function has been supplied.  However, it would be      *
 *      wise to override this function with one better match to the hash      *
 *      function which is being used.                                         *
 *                                                                            *
 *   virtual EntryCompare:                                                    *
 *      This function must be supplied by the derived class.  Failure to do   *
 *      so will result in an assert(0) being called.                          *
 *                                                                            *
 *   virtual EntryCleanup:                                                    *
 *      The default for this function is empty.                               *
 *                                                                            *
 ****************************** HashTable Private *****************************
 *                                                                            *
 * QueryIndexSet:                                                             *
 * OverflowIndexSet:                                                          *
 * OverflowEntries:                                                           *
 *                                                                            *
 * FailureToCreateError:                                                      *
 *   Prints an error message is user fails to call Create() before using the  *
 *   HashTable.                                                               *
 *                                                                            *
 * FailureToDestroyError:                                                     *
 *   Prints an error message if the user fails to call Destroy() before       *
 *   deleting the HashTable.                                                  *
 *                                                                            *
 ****************************** HashTableIterator *****************************
 *                                                                            *
 * HashTableIterator Class                                                    *
 *   Used to step through the entries in the HashTable.                       *
 *                                                                            *
 * Constructor:                                                               *
 *   Takes a pointer to the HashTable it is desired to iterate over.          *
 *                                                                            *
 * Destructor:                                                                *
 *   Nothing.                                                                 *
 *                                                                            *
 * operator ++:                                                               *
 *   Advances the iterator to the next entry in the HashTable.                *
 *                                                                            *
 * Current:                                                                   *
 *   Returns a pointer to the current entry in the HashTable pointed to by    *
 *   the iterator.                                                            *
 *                                                                            *
 * Reset:                                                                     *
 *   Resets the iterator to point to back to the first entry in the           *
 *   HashTable.                                                               *
 *                                                                            *
 *****************************************************************************/

#ifndef support_HashTable_hpp
#define support_HashTable_hpp

//************************** System Include Files ***************************

#ifdef NO_STD_CHEADERS
# include <stdarg.h>
#else
# include <cstdarg>
#endif

#include <sys/types.h>

//*************************** User Include Files ****************************


/******************* HashTable extern function prototypes ********************/

extern unsigned int IntegerHashFunct  (const int value, const unsigned int size);
extern unsigned int IntegerRehashHashFunct  (const unsigned int oldHashValue, const unsigned int size);
extern int IntegerEntryCompare  (const int value1, const int value2);

extern unsigned int StringHashFunct  (const void* entry, const unsigned int size);
extern unsigned int StringRehashFunct  (const unsigned int oldHashValue, const unsigned int size);
extern int StringEntryCompare  (const void* entry1, const void* entry2);

/*********************** HashTable function prototypes ***********************/

typedef void (*AddEntryFunctPtr)(void*, void*, va_list);
           /* void* entryCur, void* entryNew, va_list argList */

typedef void (*DeleteEntryFunctPtr)(void*, va_list);
           /* void* entry, va_list argList */

typedef unsigned int (*HashFunctFunctPtr)(const void*, const unsigned int);
           /* void* entry, unsigned int size */

typedef unsigned int (*RehashFunctFunctPtr)(const unsigned int, const unsigned int);
           /* unsigned int oldHashValue, unsigned int newSize */

typedef int (*EntryCompareFunctPtr)(const void*, const void*);
           /* void* entry1, void* entry2 */

typedef void (*EntryCleanupFunctPtr)(void*);
           /* void* entry */


/************************ HashTable class definitions ************************/

class HashTable
{
public:
  HashTable ();
  virtual ~HashTable ();

  // Must be called after creating object
  void Create (const unsigned int entrySize, unsigned int initialSize,
               HashFunctFunctPtr    const HashFunctCallback,
               RehashFunctFunctPtr  const RehashFunctCallback,
               EntryCompareFunctPtr const EntryCompareCallback,
               EntryCleanupFunctPtr const EntryCleanupCallback);

  // Must be called before deleting object
  void Destroy ();

  bool operator==(HashTable& rhsTab);

  void  AddEntry (void* entry,
                  AddEntryFunctPtr const AddEntryCallback = 0, ...);
  void  DeleteEntry (void* entry,
                     DeleteEntryFunctPtr const DeleteEntryCallback = 0, ...);
  void* QueryEntry (const void* entry) const;
  int   GetEntryIndex (const void* entry) const;
  void* GetEntryByIndex (const unsigned int index) const;
  unsigned int  NumberOfEntries () const;

  void  Dump ();

  friend class HashTableIterator;

protected:
  // Must be called after creating object
  void Create (const unsigned int entrySize, unsigned int initialSize);

  virtual unsigned int HashFunct (const void* entry, const unsigned int size);
  virtual unsigned int RehashFunct (const unsigned int oldHashValue, const unsigned int size);
  virtual int  EntryCompare (const void* entry1, const void* entry2);
  virtual void EntryCleanup (void* entry);

  HashTable& operator=(const HashTable &rhs);

private:
  const unsigned long id;                     // unique id for determining equality
  unsigned int  numSlots;                     // number of distinct symbols
  unsigned int  nextSlot;                     // next available opening
  unsigned int  entrySize;                    // byte size of the entries
  void* entries;                      // array of hash table entries
  unsigned int  indexSetSize;                 // size of sparse hash index set
  int*  indexSet;                     // sparse hash index set

  bool hashTableCreated;

  HashFunctFunctPtr    HashFunctCallback;
  RehashFunctFunctPtr  RehashFunctCallback;
  EntryCompareFunctPtr EntryCompareCallback;
  EntryCleanupFunctPtr EntryCleanupCallback;

  int  QueryIndexSet (const void* entry, const bool expand) const;
  void OverflowIndexSet ();
  void OverflowEntries ();

  void FailureToCreateError () const;
  void FailureToDestroyError () const;
};

/******************** HashTableIterator class definitions ********************/

class HashTableIterator
{
public:
  HashTableIterator(const HashTable* theHashTable);
  virtual ~HashTableIterator();

  void  operator ++(int);               // prefix
  void* Current() const;
  void  Reset();

private:
  int currentEntryNumber;
  const HashTable* hashTable;
};

/****************************** HashTableSortedIterator **********************/
#include "HashTableSortedIterator.hpp"

#endif // support_HashTable_hpp
