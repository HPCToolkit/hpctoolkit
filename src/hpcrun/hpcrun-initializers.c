// SPDX-FileCopyrightText: 2016-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

//******************************************************************************
// file: hpcrun-initializers.c
// purpose:
//   implementation of an interface for deferring a call to an initializer.
//******************************************************************************

//******************************************************************************
// local includes
//******************************************************************************

#define _GNU_SOURCE

#include "hpcrun-initializers.h"
#include "main.h"



//******************************************************************************
// local data
//******************************************************************************

static closure_list_t hpcrun_initializers = { .head = 0 };



//******************************************************************************
// interface functions
//******************************************************************************

void
hpcrun_initializers_defer(closure_t *c)
{
  if (hpcrun_is_initialized()) {
    c->fn(c->arg);
  } else {
    closure_list_register(&hpcrun_initializers, c);
  }
}


void
hpcrun_initializers_apply()
{
  closure_list_apply(&hpcrun_initializers);
}
