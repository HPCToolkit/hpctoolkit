// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//*****************************************************************************
// system includes
//*****************************************************************************

#define _GNU_SOURCE

#include "timer.h"



//*****************************************************************************
// interface operations
//*****************************************************************************

void timer_start
(
 struct timespec *start_time
)
{
    clock_gettime(CLOCK_REALTIME, start_time);
}

double
timer_elapsed
(
 struct timespec *start_time
)
{
    struct timespec end_time;
    clock_gettime(CLOCK_REALTIME, &end_time);
    return (((double) (end_time.tv_sec - start_time->tv_sec)) +  /* sec */
            ((double)(end_time.tv_nsec - start_time->tv_nsec))/1000000000); /*nanosec */
}
