// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef UNWIND_DATATYPE_H
#define UNWIND_DATATYPE_H

#if defined(HOST_CPU_x86) || defined(HOST_CPU_x86_64)
#include "../x86-family/unw-datatypes-specific.h"
#elif defined(HOST_CPU_PPC)
#include "../ppc64/unw-datatypes-specific.h"
#elif defined(HOST_CPU_ARM64) || defined(HOST_CPU_IA64)
#include "../generic-libunwind/unw-datatypes-specific.h"
#else
#error No valid HOST_CPU_* defined
#endif

#endif // UNWIND_DATATYPE_H
