// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

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

#ifndef _POSIX_SOURCE
#  define _POSIX_SOURCE /* enable clockid_t */
#endif

#include <stdlib.h>
#include <stdint.h>

#include <time.h>     /* clock_gettime() */

//*************************** User Include Files ****************************


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
  static const uint64_t microsecPerSec = 1000000;
  return (x * microsecPerSec);
}


inline static uint64_t
time_cvtNanosecToMicrosecs(uint64_t x)
{
  static const uint64_t nanosecPerMicrosec = 1000;
  return (x / nanosecPerMicrosec);
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
  // In contrast to CLOCK_MONOTONIC{_HR}, CLOCK_REALTIME{_HR} is
  // affected by NTP (network time protocol) and can move forward/backward

#ifdef CLOCK_REALTIME_HR
  return time_getTime_us(CLOCK_REALTIME_HR, time);
#else
  return time_getTime_us(CLOCK_REALTIME, time);
#endif
}


//***************************************************************************
//
//***************************************************************************

// time_getTSC: returns value of "time stamp counter" (in cycles).
//
// Benefit over clock_gettime(): In my experiments, over 10x faster
// than calling clock_gettime(): 30 cycles vs. 400 cycles.

// N.B.: Precise interpretation may be processor dependent.
inline static uint64_t
time_getTSC()
{
  uint64_t tsc = 0;

#if defined(__x86_64__)

  uint32_t hi, lo;
  asm volatile ("rdtsc" : "=a" (lo), "=d" (hi));
  tsc = (((uint64_t)hi) << 32) | ((uint64_t)lo);

#elif defined(__i386__)

  asm volatile (".byte 0x0f, 0x31" : "=A" (tsc));

#elif defined(__powerpc64__)

  asm volatile ("mftb %0" : "=r" (tsc) : );

#elif defined(__powerpc__)

  uint32_t hi, lo, tmp;
  asm volatile("0:               \n"
               "\tmftbu   %0     \n"
               "\tmftb    %1     \n"
               "\tmftbu   %2     \n"
               "\tcmpw    %2,%0  \n"
               "\tbne     0b     \n"
               : "=r" (hi), "=r" (lo), "=r" (tmp));
  tsc = (((uint64_t)hi) << 32) | ((uint64_t)lo);

#else
# warning "lib/support-lean/timer.h: time_getTSC()"
#endif

  return tsc;
}


// **************************************************************************

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* prof_lean_timer_h */
