// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef x86_build_intervals_h
#define x86_build_intervals_h

#include "x86-unwind-interval.h"

btuwi_status_t
x86_build_intervals(void *ins, unsigned int len, int noisy);

#endif
