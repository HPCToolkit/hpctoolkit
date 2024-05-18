// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

/******************************************************************************
 * File:   QuickSort Class                                                    *
 * Author: Christine Patton                                                   *
 * Date:   June, 1993                                                         *
 *                                                                            *
 ***************************** QuickSort Public *******************************
 *                                                                            *
 * Constructor:                                                               *
 *   Nothing.                                                                 *
 *                                                                            *
 * Destructor:                                                                *
 *   Nothing.                                                                 *
 *                                                                            *
 * Create:                                                                    *
 *   This function must be called after creating the object as it allocates   *
 *   space for a pointer to an array of void pointers, initializes the number *
 *   of slots in the array, and initializes a pointer to the comparison funct.*
 *   The comparison function should take two arguments and return a positive  *
 *   integer if the first argument is greater, a negative one if the second   *
 *   argument is greater.                                                     *
 *                                                                            *
 * Destroy:                                                                   *
 *   deallocates the space for the pointers initialized in Create.            *
 *                                                                            *
 * Sort:                                                                      *
 *   recursively sorts the section of the array from minEntry to maxEntry.    *
 *                                                                            *
 **************************** QuickSort Private *******************************
 *                                                                            *
 * Partition:                                                                 *
 *   returns the rank of element q int the array (all elements less than      *
 *   element q are to the left, all elements greater than element q are       *
 *   to the right)                                                            *
 *                                                                            *
 * Swap:                                                                      *
 *   pretty much self-explanatory                                             *
 *                                                                            *
 * ArrayPtr:                                                                  *
 *   pointer to the array that will be sorted                                 *
 *                                                                            *
 * CompareFunc:                                                               *
 *   pointer to the comparison function provided by the user                  *
 *                                                                            *
 * QuickSortCreated:                                                          *
 *   set to true in Create (must have value true before Sort, Partition,      *
 *   or Swap can be executed)                                                 *
 *                                                                            *
 *****************************************************************************/

#ifndef QuickSort_h
#define QuickSort_h

//************************** System Include Files ***************************

//*************************** User Include Files ****************************


/************************ QuickSort function prototypes **********************/

typedef int (*EntryCompareFunctPtr)(const void*, const void*);

/************************** QuickSort class definition ***********************/

class QuickSort
{
  public:
    QuickSort ();
    virtual ~QuickSort ();

    void Create (void** UserArrayPtr, const EntryCompareFunctPtr _CompareFunct);
    void Destroy ();

    void Sort (const int minEntryIndex, const int maxEntryIndex);

  private:
    void** ArrayPtr;
    EntryCompareFunctPtr CompareFunct;

    bool QuickSortCreated;

    void Swap (int a, int b);
    int  Partition (const int min, const int max, const int q);
};

#endif
