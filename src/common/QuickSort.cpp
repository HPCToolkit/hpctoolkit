// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

//************************** System Include Files ***************************

#ifdef NO_STD_CHEADERS
# include <stdlib.h>
#else
# include <cstdlib> // for 'rand'
using namespace std; // For compatibility with non-std C headers
#endif

//*************************** User Include Files ****************************


#include "QuickSort.hpp"

//*************************** Forward Declarations **************************

//***************************************************************************

QuickSort::QuickSort ()
{
  return;
}

QuickSort::~QuickSort ()
{
  return;
}

void QuickSort::Create (void** UserArrayPtr, const EntryCompareFunctPtr _CompareFunct)
{
  ArrayPtr = UserArrayPtr;
  CompareFunct = _CompareFunct;

  QuickSortCreated = true;

  return;
}

void QuickSort::Destroy ()
{
  QuickSortCreated = false;

  return;
}

void QuickSort::Swap (int a, int b)
{
  if (QuickSortCreated)
    {
      void* hold;
      hold = ArrayPtr[a];
      ArrayPtr[a] = ArrayPtr[b];
      ArrayPtr[b] = hold;
    }

  return;
}

int QuickSort::Partition (const int min, const int max, const int q)
{
  if (QuickSortCreated)
    {
      Swap (min, q);
      void* x = ArrayPtr[min];
      int j = min - 1;
      int k = max + 1;
      bool ExitFlag = false;
      while (!ExitFlag)
        {
          do
            k--;
          while ((CompareFunct (ArrayPtr[k], x) > 0) && (k>min));
          do
            j++;
          while ((CompareFunct (ArrayPtr[j], x) < 0) && (j<max));
          if (j < k)
             Swap (j, k);
           else
            ExitFlag = true;
        }
      return k;
    }
  else return 0;
}

void QuickSort::Sort (const int minEntryIndex, const int maxEntryIndex)
{
  if (QuickSortCreated)
    {
      if (minEntryIndex < maxEntryIndex)
        {
          int index = rand () % (maxEntryIndex-minEntryIndex+1) + minEntryIndex;
          int mid = Partition (minEntryIndex, maxEntryIndex, index);
          Sort (minEntryIndex, mid);
          Sort (mid+1, maxEntryIndex);
        }
    }

  return;
}
