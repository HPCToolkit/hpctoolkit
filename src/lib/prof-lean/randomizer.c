// -*-Mode: C++;-*- // technically C99

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
// Copyright ((c)) 2002-2022, Rice University
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

/*
 * randomizer.c
 *
 */

#include "randomizer.h"

#include "urand.h"

#include <stdlib.h>
#include <values.h>

int random_level(int max_height) {
  // generate random numbers with the required distribution by finding the
  // position of the first one in a random number. if the first one bit is
  // in position > max_height, we can wrap it back to a value in
  // [0 .. max_height-1] with a mod without disturbing the integrity of the
  // distribution.

  // a random number with the top bit set. knowing that SOME bit is set avoids
  // the need to handle the special case where no bit is set.
  unsigned int random = (unsigned int)(urand() | (1 << (INTBITS - 1)));

  // the following statement will count the trailing zeros in random.
  int first_one_position = __builtin_ctz(random);

  if (first_one_position >= max_height) {
    // wrapping a value >= maxheight with a mod operation preserves the
    // integrity of the desired distribution.
    first_one_position %= max_height;
  }

  // post-condition: first_one_pos is a random level [0 .. max_height-1]
  // with the desired distribution, assuming that urand() provides a uniform
  // random distribution.

  // return a random level [1 .. max_height] with the desired distribution.
  return first_one_position + 1;
}

#define UNIT_TEST___random_level 0
#if UNIT_TEST___random_level

#include <assert.h>
#include <stdio.h>

// test that the random levels for skip list node heights have the proper
// distribution between 1 .. max_height, where the probability of 2^h is
// half that of 2^(h-1), for h in [2 .. max_height]
int main() {
  int bins[15];

  for (int i = 0; i < 15; i++)
    bins[i] = 0;

  for (int i = 0; i < 10000 * 1024; i++) {
    int j = random_level(10);
    assert(1 <= j && j <= 15);
    bins[j - 1]++;
  }
  for (int i = 0; i < 15; i++)
    printf("bin[%d] = %d\n", i, bins[i]);
}

#endif
