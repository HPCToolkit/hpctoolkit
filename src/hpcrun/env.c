// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#define _GNU_SOURCE

#include <stdbool.h>
#include <stdlib.h>

// Names for option environment variables
const char* HPCRUN_OPT_LUSH_AGENTS = "HPCRUN_OPT_LUSH_AGENTS";

const char* HPCRUN_OUT_PATH        = "HPCRUN_OUT_PATH";
const char* HPCRUN_TRACE           = "HPCRUN_TRACE";

const char* PAPI_EVENT_LIST        = "PAPI_EVENT_LIST";

const char* HPCRUN_EVENT_LIST      = "HPCRUN_EVENT_LIST";
const char* HPCRUN_MEMSIZE         = "HPCRUN_MEMSIZE";
const char* HPCRUN_LOW_MEMSIZE     = "HPCRUN_LOW_MEMSIZE";

const char* HPCRUN_ABORT_LIBC      = "HPCRUN_ABORT_LIBC";

//
// Returns: true if 'name' is in the environment and set to a true
// (non-zero) value.
//
bool
hpcrun_get_env_bool(const char *name)
{
  if (name == NULL) { return false; }

  char * str = getenv(name);

  // not in environment
  if (str == NULL) { return false; }

  return (atoi(str) != 0);
}

bool
hpcrun_get_env_int(const char *name, int *result)
{
  if (name == NULL) { return false; }

  char * str = getenv(name);

  // not in environment
  if (str == NULL) { return false; }

  *result = atoi(str);

  return true;
}
