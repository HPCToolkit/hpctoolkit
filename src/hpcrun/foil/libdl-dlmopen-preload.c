// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//----------------------------------------------------------
// File: dlmopen.c
//
// Purpose:
//   implement dlmopen with dlopen, which has the effect of
//   loading all shared libraries in the initial namespace.
//
//   this functionality is needed when using LD_AUDIT
//   with an older glibc that crashes when using dlmopen
//   in combination with an auditor library.
//
//   this file will be compiled into a separate
//   shared library that will be loaded only when this
//   functionality is needed.
//
//----------------------------------------------------------

//******************************************************************************
// global includes
//******************************************************************************

#define _GNU_SOURCE
#include <dlfcn.h>

//******************************************************************************
// interface operations
//******************************************************************************

void* dlmopen(Lmid_t lmid, const char* filename, int flags) {
  return dlopen(filename, flags);
}
