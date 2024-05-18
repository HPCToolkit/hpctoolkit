// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//------------------------------------------------------------------------------
// File: sysv_signal.c
//
// Purpose:
//   translate calls to deprecated __sysv_signal into calls to signal,
//   which gets intercepted by libmonitor. __sysv_signal is not intercepted
//   by libmonitor.
//------------------------------------------------------------------------------

#define _GNU_SOURCE

#include "../foil.h"

#include <signal.h>
#include <stdio.h>


HPCRUN_EXPOSED __sighandler_t __sysv_signal(int signo, __sighandler_t handler)
{
  return signal(signo, handler);
}
