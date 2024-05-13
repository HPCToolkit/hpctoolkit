// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

// This file is a wrapper around libiberty's demangle.h designed to
// smooth out some quirks in the header file, some real, some
// historical.
//
// Files that want to use <libiberty/demangle.h> should include this
// file instead.
//
// We now use cplus_demangle() from libiberty again.  It has better
// options for custom demangling than abi::__cxa_demangle().

//***************************************************************************

#ifndef gnu_demangle_h
#define gnu_demangle_h

// libiberty (incorrectly) assumes that HAVE_DECL_BASENAME has been
// set from config.h
#ifndef HAVE_DECL_BASENAME
#define HAVE_DECL_BASENAME  1
#endif

#if __has_include(<libiberty/demangle.h>)
#include <libiberty/demangle.h>
#else
#include <demangle.h>
#endif

/* Undo possibly mischievous macros in binutils/include/ansidecl.h */
#undef inline

#endif
