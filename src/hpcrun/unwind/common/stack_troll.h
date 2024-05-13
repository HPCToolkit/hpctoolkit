// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef STACK_TROLL_H
#define STACK_TROLL_H


#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  TROLL_VALID  = 0, // trolled-for address provably valid
  TROLL_LIKELY,     // trolled-for address could not be proved invalid
  TROLL_INVALID     // all trolled-for addresses were invalid
} troll_status;

#include "validate_return_addr.h"
  troll_status stack_troll(void **start_sp, unsigned int *ra_pos, validate_addr_fn_t validate_addr, void *generic_arg);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif /* STACK_TROLL_H */
