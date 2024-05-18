// SPDX-FileCopyrightText: 2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

#include "../stack-entry-data.h"


#define TYPE int

typedef struct my_stack_entry_t {
  STACK_ENTRY_DATA(struct my_stack_entry_t);
  TYPE value;
} my_stack_entry_t;


/* Instantiate stack */
#define STACK_DEFINE_INPLACE
#define STACK_PREFIX          my_stack
#define STACK_ENTRY_TYPE      my_stack_entry_t
#include "../stack.h"


#include "stack-test.h"
