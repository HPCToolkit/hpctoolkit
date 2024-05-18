// SPDX-FileCopyrightText: 2012-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#ifndef SPECIFIC_INLINE_ASM_GCTXT
#define SPECIFIC_INLINE_ASM_GCTXT

// inline getcontext unknown for ia64 platform

#define INLINE_ASM_GCTXT(uc)  getcontext(&uc)

#endif // SPECIFIC_INLINE_ASM_GCTXT
