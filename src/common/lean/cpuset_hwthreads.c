// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//******************************************************************************
// system include files
//******************************************************************************

#define _GNU_SOURCE
#include <pthread.h>



//******************************************************************************
// local include files
//******************************************************************************

#include "cpuset_hwthreads.h"



//******************************************************************************
// public operations
//******************************************************************************

//------------------------------------------------------------------------------
//   Function cpuset_hwthreads
//   Purpose:
//     return the number of hardware threads available to this process
//     return 1 if no other value can be computed
//------------------------------------------------------------------------------
unsigned int
cpuset_hwthreads
(
  void
)
{
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);

  if(pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) == 0)
    return CPU_COUNT(&cpuset);

  return 1;
}
