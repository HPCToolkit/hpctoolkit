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
// Copyright ((c)) 2002-2018, Rice University
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
