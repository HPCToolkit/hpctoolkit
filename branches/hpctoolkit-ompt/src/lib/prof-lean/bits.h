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
// Copyright ((c)) 2002-2015, Rice University
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

//******************************************************************************
//
// File:
//   $HeadURL$
//
// Purpose:
//   Define common operations for examining bits in a 64-bit quantity.
//******************************************************************************

#ifndef __bits_h__
#define __bits_h__


#include <stdint.h> 

//******************************************************************************
// interface operations
//******************************************************************************

static inline uint64_t
bits_clear_first_1(uint64_t v)
{
  return v & (v-1);
}


static inline uint64_t
bits_find_first_1(uint64_t v)
{
  return v - bits_clear_first_1(v);
}


// if no bit is set, return -1
// otherwise return position of highest bit set
static inline int
bits_hibit(uint64_t v)
{
  int result = -1; 
  while (v) { result++; v>>=1; }
  return result;
}


// bits are numbered 63 .. 0
// extract bits from low to high
static inline uint64_t
bits_extract(uint64_t v, int low, int high)
{
  uint64_t top_mask = (~0ULL) >> (63-high);
  uint64_t bottom_mask = (~0ULL) << low;
  return (v & top_mask & bottom_mask);
}



//******************************************************************************
// unit test
//******************************************************************************

#define UNIT_TEST__bits__h__ 0
#if UNIT_TEST__bits__h__

#include <stdio.h>
int main()
{
  for (int v = 0; v < 32; v++)
    printf("testing ff1 %d:\n\t%0x\n\t%0x\n\n", v, v, bits_find_first_1(v));

  int v = 0;
  printf("testing hibit %d:\n\t%0x\n\t%d\n\n", v, v, bits_hibit(v));
  for (int v = 1; v <= 32; v <<=1)
    printf("testing hibit %d:\n\t%0x\n\t%d\n\n", v, v, bits_hibit(v));


  uint64_t vv = ~0ULL;
  for (int l = 0; l < 31; l++) 
    printf("testing extract(low=%d,high=%d)\n\t%016lx\n\t%016lx\n\n", l, 63, vv, bits_extract(vv,l,63));

  for (int l = 0; l < 31; l++) 
    printf("testing extract(low=%d,high=%d)\n\t%016lx\n\t%016lx\n\n", l, 63, vv, bits_extract(vv,0,63-l));

  for (int l = 0; l < 31; l++) 
    printf("testing extract(low=%d,high=%d)\n\t%016lx\n\t%016lx\n\n", l, 63, vv, bits_extract(vv,l,63-l));

  return 0;
}
#endif

#endif
