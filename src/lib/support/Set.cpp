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
// Set.C
//                                                                          
// Author:  David Whalley                                     March 1999
//
//**************************************************************************/

//************************* System Include Files ****************************

#ifdef NO_STD_CHEADERS
# include <math.h>
# include <string.h>
#else
# include <cmath>
# include <cstring>
using namespace std; // For compatibility with non-std C headers
#endif

//*************************** User Include Files ****************************

#include "Set.h"
#include "Assertion.h"

//*************************** Forward Declarations ***************************

// #define DEBUG 1

//****************************************************************************

//-------------------------------------------------------------
// implementation of class Set
//-------------------------------------------------------------

// bit mask for element
#define BITSINELEM  (sizeof(elemtype)*8)
#define ELEMBITMASK (BITSINELEM - 1)

// amount to right shift when indexing into bit string
static int elemscale = 0;


//
// Set - prepare for using a bit vector
//
Set::Set()
{
  // calculate elemscale if not already done
  if (!elemscale) {
     int word;

     word = sizeof(elemtype)*8;
     for (elemscale = -1; word; elemscale++)
        word >>= 1;
     }

  // set bit string pointer to null and length of bit string to 0
  ptr = (elemtype) NULL;
  numbitsset = 0;
  numbits = 0;
}

//
// ~Set - deallocate space for bit string
//
Set::~Set()
{
  delete ptr;
}

//
// Allocate - allocate space for bit string
//
void Set::Allocate(unsigned size)
{
  int elements = (int) ceil((double) size /
				(double) sizeof(elemtype)*8);
  ptr = new elemtype[elements];
  memset(ptr, 0, sizeof(elemtype) * elements);
  numbits = size;
}

//
// Extract - extracts a set of bits.  Assumptions are that requested bit
//           field is not longer than a "word" and that the bit field
//           does cross a word boundary (values are aligned)
//
int Set::Extract(unsigned offset, unsigned length)
{
  elemtype word;

#ifdef DEBUG
  BriefAssertion(offset+length <= numbits);
  BriefAssertion(length <= sizeof(elemtype)*8);
  BriefAssertion(offset%(sizeof(elemtype)*8) < 
		 offset%(sizeof(elemtype)*8)+length-1);
#endif

  // get the appropriate word
  word = ptr[offset >> elemscale];

  // right shift value so starting bit is in least significant position
  word >>= offset & ELEMBITMASK;

  // mask value by length of requested bit string
  return word & (((elemtype) 1 << length) - 1);
}

//
// IsBitSet - check if bit is set in a string
//
int Set::IsBitSet(unsigned offset)
{
#ifdef DEBUG
  BriefAssertion(offset < numbits);
#endif

  return ptr[offset >> elemscale] & ((elemtype) 1 << (offset & ELEMBITMASK));
}

//
// SetBit - set bit into string at specified position, returns 1 if
//          bit was not already set, returns 0 otherwise
//
int Set::SetBit(unsigned offset)
{
  int wordindex, bitinword;

#ifdef DEBUG
  BriefAssertion(offset < numbits);
#endif

  // get the appropriate word
  wordindex = offset >> elemscale;
  bitinword = ((elemtype) 1 << (offset & ELEMBITMASK));
  if (ptr[wordindex] & bitinword)
     return 1;
  else {
     ptr[wordindex] |= bitinword;
     numbitsset++;
     return 0;
     }
}

//
// Insert - insert bits into string at specified offset, returns 1 if
//          bits were not already set, returns 0 otherwise
//
int Set::Insert(unsigned offset, unsigned length)
{
  elemtype word, bitmask;
  //unsigned startbit;
  int temporal, wordindex;

#ifdef DEBUG
  BriefAssertion(offset+length <= numbits);
  BriefAssertion(length <= sizeof(elemtype)*8);
  BriefAssertion(offset%(sizeof(elemtype)*8) < 
		 offset%(sizeof(elemtype)*8)+length-1);
#endif

  // get the appropriate word
  wordindex = offset >> elemscale;
  word = ptr[wordindex];

  // mask value by length of requested bit string
  bitmask = (((elemtype) 1 << length) - 1) << (offset & ELEMBITMASK);
  word &= bitmask;

  // if all bits set, then no need to insert
  if (word == bitmask)
     return 1;

  // if empty
  else if (word == 0) {
     numbitsset += length;
     temporal = 0;
     }

  // otherwise, uncommon case
  else {
     for (word >>= (offset & ELEMBITMASK); word; word >>= 1)
        if (!(word & 1))
           numbitsset++;
     temporal = 1;
     }

  // insert the bits into the string
  ptr[wordindex] |= bitmask;

  return temporal;
}

//
// NumBitsSet - returns the number of bits set in string
//
int Set::NumBitsSet()
{
  return numbitsset;
}

//
// Clear - clear the bits in a bit string, assumes total bits was a power of 2
//         that is >= 8
//
void Set::Clear()
{
#ifdef DEBUG
  BriefAssertion(numbits >= sizeof(unsigned char) &&
		 numbits % sizeof(unsigned char) == 0);
#endif

  memset((void *) ptr, 0, numbits / sizeof(unsigned char));
  numbitsset = 0;
}
