// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2010, Rice University 
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
// File: 
//   $HeadURL$
//
// Purpose:
//   Time utilities.
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
// Author:
//   Nathan Tallent, Rice University.
//
//***************************************************************************

#ifndef prof_lean_timer_h
#define prof_lean_timer_h

//************************* System Include Files ****************************

#include <stdlib.h>
#include <stdint.h>

#include <time.h> /* clock_gettime() */

//*************************** User Include Files ****************************

#include <include/uint.h>

//*************************** Forward Declarations **************************

#if defined(__cplusplus)
extern "C" {
#endif

//***************************************************************************
// 
//***************************************************************************

inline static uint64_t
time_cvtSecToMicrosecs(uint64_t x)
{
  static const uint microsecPerSec = 1000000;
  return ((x) * microsecPerSec);
}


inline static uint64_t
time_cvtNanosecToMicrosecs(uint64_t x)
{
  static const uint nanosecPerMicrosec = 1000;
  return (x/nanosecPerMicrosec);
}


// get_timestamp_us: compute time in microseconds.  On an error,
// 'time' is guaranteed not to be changed.
inline static int
time_getTime_us(clockid_t clockid, uint64_t* time)
{
  struct timespec ts;
  long ret = clock_gettime(clockid, &ts);
  if (ret != 0) {
    return 1;
  }
  *time = (time_cvtNanosecToMicrosecs(ts.tv_nsec) 
	   + time_cvtSecToMicrosecs(ts.tv_sec));
  return 0;
}


inline static int
time_getTimeCPU(uint64_t* time)
{
  return time_getTime_us(CLOCK_THREAD_CPUTIME_ID, time);
}


inline static int
time_getTimeReal(uint64_t* time)
{
#ifdef CLOCK_REALTIME_HR
  return time_getTime_us(CLOCK_REALTIME_HR, time);
#else
  return time_getTime_us(CLOCK_REALTIME, time);
#endif
}


// **************************************************************************

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* prof_lean_timer_h */
