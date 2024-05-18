// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

#ifndef __KERNEL_BLOCKING_H__
#define __KERNEL_BLOCKING_H__

#include "perf-util.h"    // u64, u32 and perf_mmap_data_t
#include "../../sample_event.h" // sample_val_t

void kernel_blocking_init();

void
kernel_block_handler( event_thread_t *current_event, sample_val_t sv,
    perf_mmap_data_t *mmap_data);

#endif
