// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef roctracer_api_h
#define roctracer_api_h



//******************************************************************************
// interface operations
//******************************************************************************

void
roctracer_init
(
 void
);


void
roctracer_flush
(
 void *args,
 int how
);


void
roctracer_fini
(
 void *args,
 int how
);


int
roctracer_bind
(
  void
);

void
roctracer_enable_counter_collection
(
  void
);

#endif
