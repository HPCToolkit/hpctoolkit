// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef gpu_trace_channel_set_h
#define gpu_trace_channel_set_h


//******************************************************************************
// local includes
//******************************************************************************

#include "gpu-trace-channel.h"



//******************************************************************************
// forward type declarations
//******************************************************************************

typedef struct gpu_trace_channel_set_t gpu_trace_channel_set_t;



//******************************************************************************
// interface operations
//******************************************************************************

/**
 * @brief Allocates and initializes a channel set that can store up to
 * \p max_channels channels.
 * @return the newly allocated channel_set
 */
gpu_trace_channel_set_t *
gpu_trace_channel_set_new
(
 size_t max_channels
);


/**
 * @brief Calls \p apply_fn to all channels in \p channel set; current channel
 * and \p arg are passed as the arguments of \p apply_fn
 */
void
gpu_trace_channel_set_apply
(
 gpu_trace_channel_set_t *channel_set,
 void (*apply_fn)(gpu_trace_channel_t *, void *),
 void *arg
);


/**
 * @brief Waits until new messages arrived to the channels
 * or the channel is signaled.
 */
void
gpu_trace_channel_set_await
(
 gpu_trace_channel_set_t *channel_set
);


/**
 * @brief Signals \p channel_set to process its channels
*/
void
gpu_trace_channel_set_notify
(
 gpu_trace_channel_set_t *channel_set
);


/**
 * @brief Adds \p channel to \p channel_set. If \p channel cannot be added,
 * the method returns false, otherwise, it returns true.
 *
 * Thread-safe
*/
_Bool
gpu_trace_channel_set_add
(
 gpu_trace_channel_set_t *channel_set,
 gpu_trace_channel_t *channel
);

#endif
