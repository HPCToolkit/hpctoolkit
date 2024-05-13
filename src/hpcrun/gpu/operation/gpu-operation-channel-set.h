// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef gpu_operation_channel_set_h
#define gpu_operation_channel_set_h

//******************************************************************************
// forward type declarations
//******************************************************************************

typedef struct gpu_operation_channel_t gpu_operation_channel_t;
typedef struct gpu_operation_channel_set_t gpu_operation_channel_set_t;



//******************************************************************************
// interface operations
//******************************************************************************

gpu_operation_channel_set_t *
gpu_operation_channel_set_new
(
 size_t max_channels
);


_Bool
gpu_operation_channel_set_add
(
 gpu_operation_channel_set_t *channel_set,
 gpu_operation_channel_t *channel
);


void
gpu_operation_channel_set_await
(
 gpu_operation_channel_set_t *channel_set
);


void
gpu_operation_channel_set_notify
(
 gpu_operation_channel_set_t *channel_set
);


void
gpu_operation_channel_set_apply
(
 gpu_operation_channel_set_t *channel_set,
 void (*apply_fn)(gpu_operation_channel_t *, void *),
 void *arg
);


#endif
