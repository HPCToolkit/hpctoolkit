// SPDX-FileCopyrightText: 2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#include "../mpsc-queue-entry-data.h"

#define VALUE_TYPE int


typedef struct my_mpscq_entry_t {
  MPSC_QUEUE_ENTRY_DATA(struct my_mpscq_entry_t);
  int idx;
  int thread_id;
  VALUE_TYPE value;
} my_mpscq_entry_t;


/* Instantiate concurrent_stack */
#define MPSC_QUEUE_PREFIX         my_mpscq
#define MPSC_QUEUE_ENTRY_TYPE     my_mpscq_entry_t
#define MPSC_QUEUE_DEFINE_INPLACE
#include "../mpsc-queue.h"


#include "mpsc-queue-test.h"
