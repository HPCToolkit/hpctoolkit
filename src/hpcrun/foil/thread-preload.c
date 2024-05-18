// SPDX-FileCopyrightText: 2002-2023 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#define _GNU_SOURCE

#include "foil.h"
#include "../libmonitor/monitor.h"
#include <pthread.h>

HPCRUN_EXPOSED int pthread_create(pthread_t* t, const pthread_attr_t* a, pthread_start_fcn_t* s, void* d) {
  LOOKUP_FOIL_BASE(base, pthread_create);
  return base(__builtin_return_address(0), t, a, s, d);
}

HPCRUN_EXPOSED void pthread_exit(void* data) {
  LOOKUP_FOIL_BASE(base, pthread_exit);
  base(data);
  __builtin_unreachable();
}

HPCRUN_EXPOSED int pthread_sigmask(int how, const sigset_t* set, sigset_t* oldset) {
  LOOKUP_FOIL_BASE(base, pthread_sigmask);
  return base(how, set, oldset);
}
