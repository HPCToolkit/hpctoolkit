// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#define _GNU_SOURCE

#include "messages.h"

#include "../env.h"

#include "../audit/audit-api.h"

#include <signal.h>
#include <stdlib.h>

noreturn void hpcrun_terminate() {
  if (!hpcrun_get_env_bool(HPCRUN_ABORT_LIBC)) {
    struct sigaction act;
    act.sa_handler = SIG_DFL;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    auditor_exports->sigaction(SIGABRT, &act, NULL);
    auditor_exports->sigprocmask(SIG_SETMASK, &act.sa_mask, NULL);
  }
  abort();
}
