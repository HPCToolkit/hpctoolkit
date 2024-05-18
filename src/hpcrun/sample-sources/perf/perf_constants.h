// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef __PERF_CONSTANTS_H__
#define __PERF_CONSTANTS_H__

#include <unistd.h>
#include <sys/types.h>

#include <linux/types.h>
#include <linux/perf_event.h>


/******************************************************************************
 * macros
 *****************************************************************************/

#define THREAD_SELF     0
#define CPU_ANY        -1
#define GROUP_FD       -1
#define PERF_FLAGS      0
#define PERF_REQUEST_0_SKID      2
#define PERF_WAKEUP_EACH_SAMPLE  1

#define EXCLUDE    1
#define INCLUDE    0

#define EXCLUDE_CALLCHAIN EXCLUDE
#define INCLUDE_CALLCHAIN INCLUDE

#ifndef HPCRUN_DEFAULT_SAMPLE_RATE
#define HPCRUN_DEFAULT_SAMPLE_RATE        300
#endif

#ifndef u32
typedef __u32 u32;
#endif


#ifndef u64
typedef __u64 u64;
#endif


#ifndef u16
typedef __u16 u16;
#endif

#endif
