// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#define _GNU_SOURCE

#include "openmp.h"

#include "../messages/messages.h"
// The PGI OpenMP compiler does some strange things with their thread
// stacks.  We use _mp_init() as our test for this and then adjust the
// unwind heuristics if found.

void
foilbase__mp_init(void)
{
  ENABLE(OMP_SKIP_MSB);
}
