// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//
// Linux perf sample source interface
//


#ifndef __PERF_MMAP_H__
#define __PERF_MMAP_H__


/// headers
#include <linux/perf_event.h>

#include "perf-util.h"


/// types

typedef struct perf_event_header pe_header_t;


/// interfaces

void perf_mmap_init();

pe_mmap_t* set_mmap(int perf_fd);
void perf_unmmap(pe_mmap_t *mmap);

int
read_perf_buffer(pe_mmap_t *current_perf_mmap,
    struct perf_event_attr *attr, perf_mmap_data_t *mmap_info);


#endif
