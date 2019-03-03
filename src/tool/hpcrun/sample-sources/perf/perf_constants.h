// -*-Mode: C++;-*- // technically C99

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
// Copyright ((c)) 2002-2019, Rice University
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
#define HPCRUN_DEFAULT_SAMPLE_RATE	  300
#endif

#ifndef u32
typedef __u32 u32;
#endif


#ifndef u64
typedef __u64 u64;
#endif


#endif
