// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef __CUSTOM_EVENT_H__
#define __CUSTOM_EVENT_H__

#include "../../sample_event.h"
#include "../../../common/lean/hpcrun-fmt.h"


// forward type declaration from perf-util.h
struct event_info_s;
struct event_thread_s;
struct perf_mmap_data_s;

// callback functions
typedef void (*register_event_t)(kind_info_t *kb_kind, struct event_info_s *);
typedef void (*event_handler_t)(struct event_thread_s*, sample_val_t , struct perf_mmap_data_s* );


// --------------------------------------------------------------
// data structure for our customized event
// this type should be used only within perf module.
// --------------------------------------------------------------
typedef struct event_custom_s {
  const char *name;            // unique name of the event
  const char *desc;            // brief description of the event

  register_event_t register_fn;// function to register the event
  event_handler_t  handler_fn; // callback to be used during the sampling

  int            metric_index; // hpcrun's index metric
  metric_desc_t *metric_desc;  // pointer to predefined metric
} event_custom_t;

event_custom_t *event_custom_find(const char *name);
int event_custom_register(event_custom_t *event);
void event_custom_display(FILE *std);

#endif
