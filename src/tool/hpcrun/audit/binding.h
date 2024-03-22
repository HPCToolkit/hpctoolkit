// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2024, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

#ifndef AUDIT_BINDING_H
#define AUDIT_BINDING_H

#include <stdarg.h>

/// Dlopen a shared library into the main linkage namespace and bind symbols from it.
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
/// The library `libname` will be loaded into the main (application) linkage namespace. It will
/// access the same global variables as the application, but any symbols it calls may be wrapped by
/// us or other active profilers/tools in the address space. Internal `hpcrun` functions may be
/// unavailable. To avoid undefined behavior when external dependencies change ABI, `libname` should
/// ALWAYS be a library we build and install, e.g. "libhpcrun_*.so", which should itself be linked
/// to the external libraries in question.
///
/// Note that libraries loaded via this function can never be unloaded.
inline void hpcrun_bind(const char* libname, ...);

/// Variant of #hpcrun_bind that accepts a `va_list` for varadic arguments.
void hpcrun_bind_v(const char* libname, va_list bindings);

inline void hpcrun_bind(const char* libname, ...) {
  va_list bindings;
  va_start(bindings, libname);
  hpcrun_bind_v(libname, bindings);
  va_end(bindings);
}

/// Dlopen a shared library into the main linkage namespace.
///
/// This is a DEPRECATED function only for refactoring legacy code. New code should use #hpcrun_bind
/// instead of this function.
void* hpcrun_raw_dlopen(const char* libname, int flags);

/// Dlclose a shared library in the main linkage namespace, returned by #hpcrun_raw_dlopen.
///
/// This is a DEPRECATED function only for refactoring legacy code. New code should not use this
/// function and instead use #hpcrun_bind to load libraries into the application namespace.
void hpcrun_raw_dlclose(void* handle);

/// Dlopen a shared library into the private linkage namespace and bind symbols from it.
///
/// See #hpcrun_bind for syntax and error handling.
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
