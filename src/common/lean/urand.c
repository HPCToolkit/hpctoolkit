// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

//******************************************************************************
//
// File:
//   $HeadURL$
//
// Purpose:
//   Implement an API for generating thread-local streams of uniform random
//   numbers.
//
//******************************************************************************



//******************************************************************************
// global includes
//******************************************************************************

#include <stdio.h>

#define __USE_POSIX 1  // needed to get decl for rand_r with -std=c99

#include <stdlib.h>



//******************************************************************************
// local includes
//******************************************************************************

#include "usec_time.h"


//******************************************************************************
// macros
//******************************************************************************

#define LOW_32BITS ((unsigned int) ~0)



//******************************************************************************
// local thread private data
//******************************************************************************

static __thread int urand_initialized = 0;
static __thread unsigned int urand_data;



//******************************************************************************
// interface functions
//******************************************************************************

// Function: urand
//
// Purpose:
//   generate a pseudo-random number between [0 .. RAND_MAX]. a thread-local
//   seed is initialized using the time of day in microseconds when a thread
//   first calls the generator.
//
// NOTE:
//   you might be tempted to use srandom_r and random_r instead. my experience
//   is that this led to a SIGSEGV when used according to the man page on an
//   x86_64. this random number generator satisfies our modest requirements.
int
urand()
{
  if (!urand_initialized) {
    urand_data = usec_time() & LOW_32BITS;
    urand_initialized = 1;
  }
  return rand_r(&urand_data);
}


// generate a pseudo-random number [0 .. n], where n <= RAND_MAX
int
urand_bounded(int n)
{
  return urand() % n;
}
