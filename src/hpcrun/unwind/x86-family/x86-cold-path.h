// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef X86_COLD_PATH_H
#define X86_COLD_PATH_H

#include <stdbool.h>
#include "x86-unwind-analysis.h"
#include "x86-interval-arg.h"

extern bool hpcrun_is_cold_code(xed_decoded_inst_t *xptr, interval_arg_t *iarg);
extern void hpcrun_cold_code_fixup(unwind_interval *first, unwind_interval *current, unwind_interval *warm);

#endif // X86_COLD_PATH_H
