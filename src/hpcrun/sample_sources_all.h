// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef ALL_SAMPLE_SOURCES_H
#define ALL_SAMPLE_SOURCES_H

#define MAX_SAMPLE_SOURCES 2
#define MAX_HARDWARE_SAMPLE_SOURCES 1

#include <stdbool.h>
#include <stddef.h>

#include "sample-sources/sample_source_obj.h"

extern void hpcrun_sample_sources_from_eventlist(char *evl);
extern sample_source_t* hpcrun_fetch_source_by_name(const char *src);
extern bool hpcrun_check_named_source(const char *src);
extern void hpcrun_all_sources_init(void);
extern void hpcrun_all_sources_thread_init(void);
extern void hpcrun_all_sources_thread_init_action(void);
extern void hpcrun_all_sources_process_event_list(int lush_metrics);
extern void hpcrun_all_sources_finalize_event_list(void);
extern void hpcrun_all_sources_gen_event_set(int lush_metrics);
extern void hpcrun_all_sources_start(void);
extern void hpcrun_all_sources_thread_fini_action(void);
extern void hpcrun_all_sources_stop(void);
extern void hpcrun_all_sources_shutdown(void);
extern bool  hpcrun_all_sources_started(void);
extern size_t hpcrun_get_num_sample_sources(void);

#define SAMPLE_SOURCES(op,...) hpcrun_all_sources_ ##op(__VA_ARGS__)

#endif // ALL_SAMPLE_SOURCES_H
