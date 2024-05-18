// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#define _GNU_SOURCE

#include "safe-sampling.h"

// functions exported to a separate library namespace
int
hpcrun_safe_enter_noinline(void)
{
  return hpcrun_safe_enter();
}

void
hpcrun_safe_exit_noinline(void)
{
  hpcrun_safe_exit();
}
