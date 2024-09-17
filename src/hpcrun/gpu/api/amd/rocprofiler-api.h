// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef rocprofiler_api_h
#define rocprofiler_api_h

//******************************************************************************
// global includes
//******************************************************************************

#include <stdint.h>


//******************************************************************************
// macro definitions
//******************************************************************************

#define ROCTRACER_CHANNEL_IDX 0
#define ROCPROFILER_CHANNEL_IDX 1

//******************************************************************************
// interface operations
//******************************************************************************

void
rocprofiler_start_kernel
(
 uint64_t correlation_id
);


void
rocprofiler_stop_kernel
(
 void
);


void
rocprofiler_init
(
 void
);


void
rocprofiler_fini
(
 void *args,
 int how
);

void
rocprofiler_wait_context_callback
(
  void
);

int
rocprofiler_total_counters
(
  void
);

const char*
rocprofiler_counter_name
(
  int idx
);

const char*
rocprofiler_counter_description
(
  int idx
);

int
rocprofiler_match_event
(
  const char *ev_str
);

void
rocprofiler_finalize_event_list
(
  void
);

void
rocprofiler_uri_setup
(
  void
);

void
rocprofiler_register_counter_callbacks
(
  void
);

// NB: The argument is a pointer to a rocprofiler_settings_t
void hpcrun_OnLoadToolProp(void* settings);

void hpcrun_OnUnloadTool();


#endif
