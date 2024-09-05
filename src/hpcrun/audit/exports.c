// SPDX-FileCopyrightText: 2002-2023 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
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
void export_symbols(auditor_exports_t* exports) {
  exports->exit = exit;
  exports->sigprocmask = sigprocmask;
  exports->pthread_sigmask = pthread_sigmask;
  exports->sigaction = sigaction;
}
