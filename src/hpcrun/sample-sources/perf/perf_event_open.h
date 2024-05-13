// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef __PERF_EVENT_OPEN_H__
#define __PERF_EVENT_OPEN_H__

#include <unistd.h>             // pid_t
#include <linux/perf_event.h>   // perf data structure

long
perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
         int cpu, int group_fd, unsigned long flags);

#endif
