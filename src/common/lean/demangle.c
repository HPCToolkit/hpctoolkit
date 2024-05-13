// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

// This file provides a wrapper around libiberty's cplus_demangle() to
// provide a uniform interface for the options that we want for
// hpcstruct and hpcprof.  All cases wanting to do demangling should
// use this file.
//
// Libiberty cplus_demangle() does many malloc()s, but does appear to
// be reentrant and thread-safe.  But not signal safe.

//***************************************************************************

#include <string.h>

#include "gnu_demangle.h"
#include "spinlock.h"
#include "hpctoolkit_demangle.h"

#define DEMANGLE_FLAGS  (DMGL_PARAMS | DMGL_ANSI | DMGL_VERBOSE | DMGL_RET_DROP)

static spinlock_t demangle_lock = SPINLOCK_UNLOCKED;

// Returns: malloc()ed string for the demangled name, or else NULL if
// 'name' is not a mangled name.
//
// Note: the caller is responsible for calling free() on the result.
//
char *
hpctoolkit_demangle(const char * name)
{
  if (name == NULL) {
    return NULL;
  }

  if (strncmp(name, "_Z", 2) != 0) {
    return NULL;
  }

  // NOTE: comments in GCC demangler indicate that the demangler uses shared state
  // and that locking for multithreading is our responsibility
  spinlock_lock(&demangle_lock);
  char *demangled = cplus_demangle(name, DEMANGLE_FLAGS);
  spinlock_unlock(&demangle_lock);

  return demangled;
}
