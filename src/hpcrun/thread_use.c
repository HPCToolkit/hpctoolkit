// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//******************************************************************************
// system include files
//******************************************************************************

#define _GNU_SOURCE

#include <stdbool.h>
#include <inttypes.h>

//******************************************************************************
// local include files
//******************************************************************************

#include "thread_use.h"



//******************************************************************************
// local variables
//******************************************************************************
static bool using_threads;



//******************************************************************************
// interface operations
//******************************************************************************

void
hpcrun_set_using_threads(bool flag)
{
  using_threads = flag;
}


bool
hpcrun_using_threads_p(void)
{
  return using_threads;
}
