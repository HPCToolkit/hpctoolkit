// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef HPCFNBOUNDS_FUNCTION_ENTRIES_H
#define HPCFNBOUNDS_FUNCTION_ENTRIES_H

#include <sys/types.h>
#include <inttypes.h>
#include <stdbool.h>
#include "code-ranges.h"

int c_mode(void);

void function_entries_reinit();

void add_function_entry(void *address, const char *comment, bool isglobal, int call_count);
void add_stripped_function_entry(void *function_entry, int call_count);
bool contains_function_entry(void *address);

void add_protected_range(void *start, void *end);
int  is_possible_fn(void *addr);
int  inside_protected_range(void *addr);

void entries_in_range(void *start, void *end, void **result);
bool query_function_entry(void *addr);

void dump_reachable_functions();

#endif  // HPCFNBOUNDS_FUNCTION_ENTRIES_H
