// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#define _GNU_SOURCE

#define UNW_LOCAL_ONLY

#include <sys/types.h>
#include <libunwind.h>

void *
hpcrun_context_pc_async(void *context)
{
  // We would need to start up libunwind to do this, and that can't be
  // async-signal-safe since libunwind needs to malloc internal data.
  return NULL;
}
