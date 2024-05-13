// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef code_ranges_hpp
#define code_ranges_hpp

#include <stdio.h>

typedef enum DiscoverFnTy {
  DiscoverFnTy_NULL = 0,
  DiscoverFnTy_Aggressive,
  DiscoverFnTy_Conservative,
  DiscoverFnTy_None
} DiscoverFnTy;

void code_ranges_reinit();

#endif // code_ranges_hpp
