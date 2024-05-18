// SPDX-FileCopyrightText: 2002-2023 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

#define _GNU_SOURCE

#include "audit-api.h"

#include <unistd.h>
#include <sys/wait.h>
#include <sched.h>
#include <stdlib.h>
#include <signal.h>

__attribute__((visibility("default")))
void fill_exports(auditor_exports_t* exports) {
  exports->pipe = pipe;
  exports->close = close;
  exports->waitpid = waitpid;
  exports->clone = clone;
  exports->execve = execve;
  exports->exit = exit;
  exports->sigprocmask = sigprocmask;
  exports->pthread_sigmask = pthread_sigmask;
  exports->sigaction = sigaction;
}
