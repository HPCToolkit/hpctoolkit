// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef gpu_activity_channel_h
#define gpu_activity_channel_h

//******************************************************************************
// local includes
//******************************************************************************

#include "gpu-activity.h"



//******************************************************************************
// forward type declarations
//******************************************************************************

typedef struct gpu_activity_channel_t gpu_activity_channel_t;



//******************************************************************************
// interface operations
//******************************************************************************

/**
 * Generate a correlation ID associated with the thread-local channel
 */
uint64_t
gpu_activity_channel_generate_correlation_id
(
 void
);


/**
 * Extract thread ID from correlation ID
 */
uint32_t
gpu_activity_channel_correlation_id_get_thread_id
(
 uint64_t correlation_id
);


/**
 * Returns the thread-local channel. If the channel is not initialized,
 * it first initializes thread-local channel
 */
gpu_activity_channel_t *
gpu_activity_channel_get_local
(
 void
);


/**
 * Returns channel whose ID is \p id. If channel is not found,
 * returns NULL.
 * The functions is thread safe.
 */
gpu_activity_channel_t *
gpu_activity_channel_lookup
(
 uint32_t thread_id
);


/**
 * Sends \p activity to \p channel
 */
void
gpu_activity_channel_send
(
 gpu_activity_channel_t *channel,
 const gpu_activity_t *activity
);


/**
 * Calls \p receive_fn for each activity in the thread-local channel;
 * each activity is passed as the argument of \p receive_fn() .
 * All activities passed to \p receive_fn are removed from the channel.
 */
void
gpu_activity_channel_receive_all
(
 void (*receive_fn)(gpu_activity_t *)
);


#endif
