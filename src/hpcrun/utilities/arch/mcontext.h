// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef MCONTEXT_H
#define MCONTEXT_H

#if defined(HOST_CPU_x86) || defined(HOST_CPU_x86_64)
#include "x86-family/_mcontext.h"
#elif defined(HOST_CPU_PPC)
#include "ppc64/_mcontext.h"
#elif defined(HOST_CPU_IA64)
#include "ia64/_mcontext.h"
#elif defined(HOST_CPU_ARM64)
#include "aarch64/_mcontext.h"
#else
#error No valid HOST_CPU_* defined
#endif

#endif // MCONTEXT_H
