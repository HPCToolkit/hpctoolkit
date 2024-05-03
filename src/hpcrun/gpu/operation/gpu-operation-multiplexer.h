// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

#ifndef gpu_operation_multiplexer_h
#define gpu_operation_multiplexer_h

#include "../../thread_data.h"
#include "gpu-operation-channel.h"

#ifndef __cplusplus
#include <stdatomic.h>
#else
#include <atomic>
#endif


//******************************************************************************
// type declarations
//******************************************************************************
typedef struct gpu_activity_channel_t gpu_activity_channel_t;
typedef struct gpu_activity_t gpu_activity_t;



//******************************************************************************
// interface operations
//******************************************************************************

void
gpu_operation_multiplexer_fini
(
 void
);


void
gpu_operation_multiplexer_push
(
 gpu_activity_channel_t *initiator_channel,
#ifdef __cplusplus
 std::
#endif
 atomic_int *initiator_pending_operations,
 gpu_activity_t *gpu_activity
);


#endif
