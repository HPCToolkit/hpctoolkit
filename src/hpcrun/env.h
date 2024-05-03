// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef hpcrun_env_h
#define hpcrun_env_h

#include <stdbool.h>

// Names for option environment variables
extern const char* HPCRUN_OPT_LUSH_AGENTS;

extern const char* HPCRUN_OUT_PATH;

extern const char* HPCRUN_TRACE;

extern const char* HPCRUN_EVENT_LIST;
extern const char* HPCRUN_MEMSIZE;
extern const char* HPCRUN_LOW_MEMSIZE;

extern const char* HPCRUN_ABORT_LIBC;

bool hpcrun_get_env_bool(const char *);

bool hpcrun_get_env_int(const char *, int *);

#endif /* hpcrun_env_h */
