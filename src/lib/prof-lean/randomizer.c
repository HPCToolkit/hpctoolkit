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

#define UNIT_TEST___random_level 0
#if UNIT_TEST___random_level

#include <stdio.h>
#include <assert.h>

// test that the random levels for skip list node heights have the proper
// distribution between 1 .. max_height, where the probability of 2^h is
// half that of 2^(h-1), for h in [2 .. max_height]
int main()
{
  int bins[15];

  for (int i = 0; i < 15; i++) bins[i] = 0;

  for (int i = 0; i < 10000 * 1024 ; i++) {
    int j = random_level(10);
    assert(1 <= j && j <= 15);
    bins[j-1]++;
  }
  for (int i = 0; i < 15;i++)
    printf("bin[%d] = %d\n", i, bins[i]);
}

#endif

