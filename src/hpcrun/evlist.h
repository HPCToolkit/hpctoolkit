// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef EVLIST_H
#define EVLIST_H

#define MAX_EVL_SIZE 1024
#define MAX_EVENTS  50

typedef struct _ev_t {
  int event;
  long thresh;
  int metric_id;
} _ev_t;


typedef struct evlist_t {
  char evl_spec[MAX_EVL_SIZE];
  int nevents;
  _ev_t events[MAX_EVENTS];
} evlist_t;

#endif // EVLIST_H
