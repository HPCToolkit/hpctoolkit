// SPDX-FileCopyrightText: 2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

#ifndef GENERIC_STACK_ENTRY_DATA
#define GENERIC_STACK_ENTRY_DATA 1

#define STACK_ENTRY_DATA(ENTRY_TYPE)    \
struct {                                \
  ENTRY_TYPE *next;                     \
}

#endif
