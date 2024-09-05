// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#define _GNU_SOURCE

#include "binding.h"

#include "../messages/messages.h"
#include "audit-api.h"

#include <dlfcn.h>
#include <stddef.h>

extern inline void hpcrun_bind(const char* libname, ...);
extern inline void hpcrun_bind_private(const char* libname, ...);

// libhpcrun.so is in the main namespace, so we can handle it with dlopen.
void hpcrun_bind_v(const char* libname, va_list bindings) {
  // Before anything else, try to load the library.
  void* handle = dlopen(libname, RTLD_NOW | RTLD_LOCAL);
  if (handle == NULL) {
    EEMSG("Unable to bind to '%s': failed to load: %s", libname, dlerror());
    hpcrun_terminate();
  }

  // Bind each requested symbol in turn
  dlerror();  // Clear any errors
  for (const char* sym = va_arg(bindings, const char*); sym != NULL;
       sym = va_arg(bindings, const char*)) {
    void** dst = va_arg(bindings, void**);
    *dst = dlsym(handle, sym);
    const char* error = dlerror();
    if (*dst == NULL && error != NULL) {
      EEMSG("Unable to bind to '%s': %s", libname, error);
      hpcrun_terminate();
    }
  }
}

void* hpcrun_raw_dlopen(const char *libname, int flags) {
  return dlopen(libname, flags);
}

void hpcrun_raw_dlclose(void* handle) {
  dlclose(handle);
}

// In the dynamic case, the private namespace is the auditor's namespace, and as such
// hpcrun_bind_private_v is implemented by the the auditor API.
void hpcrun_bind_private_v(const char* libname, va_list bindings) {
  return auditor_exports()->hpcrun_bind_v(libname, bindings);
}
