// -*-Mode: C++;-*- // technically C99
// $Id$

//***************************************************************************
//
// File: 
//   $Source$
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
