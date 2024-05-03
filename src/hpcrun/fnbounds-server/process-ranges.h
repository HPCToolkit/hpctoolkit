// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef process_ranges_hpp
#define process_ranges_hpp

#include "code-ranges.h"

void process_range_init();

void process_range(const char *name, long offset, void *vstart, void *vend,
                   DiscoverFnTy fn_discovery);

bool range_contains_control_flow(void *vstart, void *vend);

#endif // process_ranges_hpp
