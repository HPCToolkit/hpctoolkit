// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef cupti_analysis_h
#define cupti_analysis_h

#include <cupti_activity.h>

void
cupti_occupancy_analyze
(
 CUpti_ActivityKernel4 *activity,
 uint32_t *active_warps_per_sm,
 uint32_t *max_active_warps_per_sm,
 uint32_t *thread_registers,
 uint32_t *block_threads,
 uint32_t *block_shared_memory
);


void
cupti_sm_efficiency_analyze
(
 CUpti_ActivityPCSamplingRecordInfo *pc_sampling_record_info,
 uint64_t *total_samples,
 uint64_t *full_sm_samples
);



#endif
