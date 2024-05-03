// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

//******************************************************************************
//
// File:
//   $HeadURL$
//
// Purpose:
//   Implement a function that returns the time of day in microseconds as a
//   long integer.
//
//******************************************************************************



//******************************************************************************
// global includes
//******************************************************************************

#include <stdlib.h>
#include <sys/time.h>



//******************************************************************************
// local includes
//******************************************************************************

#include "usec_time.h"



//******************************************************************************
// macros
//******************************************************************************

#define USEC_PER_SEC 1000000



//******************************************************************************
// interface functions
//******************************************************************************

// return the time of day in microseconds as a long integer
unsigned long
usec_time()
{
  struct timeval tv;
  int retval = gettimeofday(&tv, 0);
  if (retval != 0)
    abort();  // gettimeofday failed
  return tv.tv_usec + USEC_PER_SEC * tv.tv_sec;
}
