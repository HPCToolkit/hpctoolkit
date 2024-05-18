/*
 * randomizer.h
 *
 */

#ifndef __RANDOMIZER_H__
#define __RANDOMIZER_H__

// generate a random level for a skip list node in the interval
// [1 .. max_height]. these random numbers are distributed such that the
// probability for each height h is half that of height h - 1, for h in
// [2 .. max_height].
int
random_level(int max_height);


#endif /* __RANDOMIZER_H__ */
