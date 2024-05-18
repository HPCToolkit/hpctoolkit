// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef hpcrun_trace_h
#define hpcrun_trace_h
#include <stdint.h>
#include "files.h"
#include "core_profile_trace_data.h"


typedef enum hpcrun_trace_type_masks {
    HPCRUN_CPU_TRACE_FLAG = 1,
    HPCRUN_GPU_TRACE_FLAG = 2,
    HPCRUN_CPU_KERNEL_LAUNCH_TRACE_FLAG = 4
} hpcrun_trace_type_masks_t;

typedef enum hpcrun_trace_type {
    HPCRUN_NO_TRACE,
    HPCRUN_SAMPLE_TRACE,
    HPCRUN_CALL_TRACE,
} hpcrun_trace_type_t;

void trace_other_close(void *thread_data);

void hpcrun_trace_init();
void hpcrun_trace_open(core_profile_trace_data_t * cptd, hpcrun_trace_type_t type);
void hpcrun_trace_append(core_profile_trace_data_t *cptd, cct_node_t* node, unsigned int metric_id, uint32_t dLCA, uint64_t sampling_period);
void hpcrun_trace_append_with_time(core_profile_trace_data_t *st, unsigned int call_path_id, unsigned int metric_id, uint64_t nanotime);
void hpcrun_trace_close(core_profile_trace_data_t * cptd);

void hpcrun_trace_append_stream(core_profile_trace_data_t *cptd, cct_node_t *node, unsigned int metric_id, uint32_t dLCA, uint64_t nanotime);

int hpcrun_trace_isactive();

// When a sample source finds that a metric suitable for tracing
// is specified, we call this function to indicate we can do tracing
void hpcrun_set_trace_metric(hpcrun_trace_type_masks_t);

// After processing all command line events, we call this function
// to check whether a metric suitable for tracing exists
int hpcrun_has_trace_metric();

int hpcrun_cpu_trace_on();
int hpcrun_gpu_trace_on();
int hpcrun_cpu_kernel_launch_trace_on();

#endif // hpcrun_trace_h
