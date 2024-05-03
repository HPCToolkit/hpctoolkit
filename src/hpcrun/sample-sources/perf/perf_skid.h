// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef __PERF_SKID_H__
#define __PERF_SKID_H__

// constants of precise_ip (see the man page)
#define PERF_EVENT_AUTODETECT_SKID       4
#define PERF_EVENT_SKID_ZERO_REQUIRED    3
#define PERF_EVENT_SKID_ZERO_REQUESTED   2
#define PERF_EVENT_SKID_CONSTANT         1
#define PERF_EVENT_SKID_ARBITRARY        0
#define PERF_EVENT_SKID_ERROR           -1


// parse the event into event_name and the type of precise_ip
//  the name of the event excludes the precise ip suffix
// returns:
//  PERF_EVENT_AUTODETECT_SKID
//  PERF_EVENT_SKID_ZERO_REQUIRED
//  PERF_EVENT_SKID_ZERO_REQUESTED
//  PERF_EVENT_SKID_CONSTANT
//  PERF_EVENT_SKID_ARBITRARY
//  PERF_EVENT_SKID_NONE
//  PERF_EVENT_SKID_ERROR
int
perf_skid_parse_event(const char *event_string, char **event_string_without_skidmarks);

int
perf_skid_set_max_precise_ip(struct perf_event_attr *attr);

u64
perf_skid_get_precise_ip(struct perf_event_attr *attr);
#endif
