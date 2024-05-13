// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef gpu_correlation_channel_h
#define gpu_correlation_channel_h



//******************************************************************************
// global includes
//******************************************************************************

#include <stdint.h>



//******************************************************************************
// type declarations
//******************************************************************************

typedef struct gpu_correlation_channel_t gpu_correlation_channel_t;

typedef struct gpu_activity_channel_t gpu_activity_channel_t;



//******************************************************************************
// interface operations
//******************************************************************************

void
gpu_correlation_channel_send
(
 uint64_t idx,
 uint64_t host_correlation_id,
 gpu_activity_channel_t *activity_channel
);


void
gpu_correlation_channel_receive
(
 uint64_t idx,
 void (*receive_fn)(uint64_t, gpu_activity_channel_t *, void *),
 void *arg
);


#endif
