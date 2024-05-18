// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef __PERFMON_UTIL_H__
#define __PERFMON_UTIL_H__

#include <linux/perf_event.h>
#include "perf-util.h"    // u64, u32 and perf_mmap_data_t

int pfmu_init();
int pfmu_showEventList();
int pfmu_isSupported(const char *eventname);
int pfmu_getEventType(const char *eventname, u64 *code, u64 *type);
int pfmu_getEventAttribute(const char *eventname, struct perf_event_attr *event_attr);
const char* pfmu_getEventDescription(const char *event_name);

void pfmu_fini();

#endif
