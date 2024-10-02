// SPDX-FileCopyrightText: 2016-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

/*
 * randomizer.c
 *
 */

//******************************************************************************
// global includes
//******************************************************************************

#include <stdlib.h>
#include <values.h>


//******************************************************************************
// local includes
//******************************************************************************

#include "urand.h"
#include "randomizer.h"


int
random_level(int max_height)
{
  // generate random numbers with the required distribution by finding the
  // position of the first one in a random number. if the first one bit is
  // in position > max_height, we can wrap it back to a value in
  // [0 .. max_height-1] with a mod without disturbing the integrity of the
  // distribution.

  // a random number with the top bit set. knowing that SOME bit is set avoids
  // the need to handle the special case where no bit is set.
  unsigned int random = (unsigned int) (urand() | (1 << (INTBITS - 1)));

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
