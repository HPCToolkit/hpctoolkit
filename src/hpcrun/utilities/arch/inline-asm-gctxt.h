// SPDX-FileCopyrightText: 2011-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

#ifndef INLINE_ASM_GCTXT_H
#define INLINE_ASM_GCTXT_H

#if defined(HOST_CPU_x86) || defined(HOST_CPU_x86_64)
#include "x86-family/specific-inline-asm-gctxt.h"
#elif defined(HOST_CPU_PPC)
#include "ppc64/specific-inline-asm-gctxt.h"
#elif defined(HOST_CPU_IA64)
#include "ia64/specific-inline-asm-gctxt.h"
#elif defined(HOST_CPU_ARM64)
#include "aarch64/specific-inline-asm-gctxt.h"
#else
#error No valid HOST_CPU_* defined
#endif

#endif // INLINE_ASM_GCTXT_H
