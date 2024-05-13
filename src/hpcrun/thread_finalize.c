// SPDX-FileCopyrightText: 2015-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

//******************************************************************************
// local includes
//******************************************************************************

#define _GNU_SOURCE

#include "thread_finalize.h"



//******************************************************************************
// global variables
//******************************************************************************

static thread_finalize_entry_t *finalizers;




//******************************************************************************
// interface operations
//******************************************************************************

void
thread_finalize_register(
  thread_finalize_entry_t *e
)
{
  e->next = finalizers;
  finalizers = e;
}


void
thread_finalize
(
  int is_process
)
{
 thread_finalize_entry_t *e = finalizers;
  while (e) {
    e->fn(is_process);
    e = e->next;
  }
}
