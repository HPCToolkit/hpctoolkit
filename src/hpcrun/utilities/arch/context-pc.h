// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef CONTEXT_PC
#define CONTEXT_PC

#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>

#include "mcontext.h"

/// Fetch the current address (PC) out of the arch-specific context structure,
/// if can be done async-signal-safely. Otherwise returns a constant NULL.
void* hpcrun_context_pc_async(void* context);


#endif // CONTEXT_PC
