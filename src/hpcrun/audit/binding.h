// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef AUDIT_BINDING_H
#define AUDIT_BINDING_H

#include <stdarg.h>

/// Dlopen a shared library into the private linkage namespace and bind symbols from it.
///
/// This function takes a variable number of paired arguments, each pair consists of a symbol name
/// (`const char* sym`) and destination pointer (`void** dst`). This list should be terminated by a
/// `NULL` pointer. For example:
///
/// ```c
/// typedef void (*pfn_foo_t)();
/// pfn_foo_t foo = NULL;
/// hpcrun_bind("libhpcrun_foo.so",
///             "foo", &pfn_foo,
///             // ...
///             NULL);
/// ```
///
/// All failures are considered fatal: an error is reported and the process aborted if the library
/// `libname` or any symbol `sym` is not found, or if the library cannot be loaded for any reason.
///
/// The library `libname` will be loaded into the private linkage namespace. Any global variables it
/// accesses will be separate from the application but shared among other binaries in the private
/// namespace. Any symbols it calls will not be wrapped (i.e. symbol wrapping is evaded). Internal
/// `hpcrun` functions may be unavailable. To avoid undefined behavior when external dependencies
/// change ABI, `libname` should ALWAYS be a library we build and install, e.g. "libhpcrun_*.so",
/// which should itself be linked to the external libraries in question.
///
/// Note that libraries loaded via this function can never be unloaded.
inline void hpcrun_bind_private(const char* libname, ...);

/// Variant of #hpcrun_bind_private that accepts a `va_list` for varadic arguments.
void hpcrun_bind_private_v(const char* libname, va_list bindings);

inline void hpcrun_bind_private(const char* libname, ...) {
  va_list bindings;
  va_start(bindings, libname);
  hpcrun_bind_private_v(libname, bindings);
  va_end(bindings);
}

#endif  // AUDIT_BINDING_H
