// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

// This file provides a wrapper around libiberty's cplus_demangle() to
// provide a uniform interface for the options that we want for
// hpcstruct and hpcprof.  All cases wanting to do demangling should
// use this file.

//***************************************************************************

#ifndef support_lean_demangle_h
#define support_lean_demangle_h

#if defined(__cplusplus)
extern "C" {
#endif

// Returns: malloc()ed string for the demangled name, or else NULL.
// Note: the caller is responsible for calling free() on the result.
//
char * hpctoolkit_demangle(const char *);

#if defined(__cplusplus)
}
#endif

#endif
