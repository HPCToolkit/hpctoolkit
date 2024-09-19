// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#define _GNU_SOURCE

#include "binding.h"

#include "audit-api.h"

extern inline void hpcrun_bind_private(const char* libname, ...);

// In the dynamic case, the private namespace is the auditor's namespace, and as such
// hpcrun_bind_private_v is implemented by the the auditor API.
void hpcrun_bind_private_v(const char* libname, va_list bindings) {
  return auditor_exports()->hpcrun_bind_v(libname, bindings);
}
