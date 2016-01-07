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

//************************** System Include Files ***************************

#ifdef NO_STD_CHEADERS
# include <stdlib.h>
#else
# include <cstdlib> // for 'rand'
using namespace std; // For compatibility with non-std C headers
#endif

//*************************** User Include Files ****************************

#include <include/uint.h>

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

