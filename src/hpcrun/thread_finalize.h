// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef __thread_finalize_h__
#define __thread_finalize_h__



//******************************************************************************
// type declarations
//******************************************************************************

typedef void (*thread_finalize_fn)
(
  int is_process
);


typedef struct thread_finalize_entry_s {
  struct thread_finalize_entry_s *next;
  thread_finalize_fn fn;
} thread_finalize_entry_t;



//******************************************************************************
// interface operations
//******************************************************************************

extern void
thread_finalize_register
(
  thread_finalize_entry_t *e
);


extern void
thread_finalize
(
  int is_process
);


#endif
