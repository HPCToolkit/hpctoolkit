// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef EVENT_INFO_H
#define EVENT_INFO_H

#include "sample-sources/sample_source_obj.h"

typedef struct {
  sample_source_t sample_source; /* what kind of event is used */
  unsigned long sample_period; /* when itimer is used */
  char *event_list;    /* string repr list of events (including papi events) */
} event_info;

#endif // EVENT_INFO_H
