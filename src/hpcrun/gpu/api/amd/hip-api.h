// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//***************************************************************************
//
// File:
//   cuda-api.h
//
// Purpose:
//   interface definitions for wrapper around AMD HIP layer
//
//***************************************************************************

#ifndef hip_api_h
#define hip_api_h



//*****************************************************************************
// rocm includes
//*****************************************************************************

#include <hip/hip_runtime.h>



//*****************************************************************************
// interface operations
//*****************************************************************************

typedef struct hip_device_property {
 int sm_count;
 int sm_clock_rate;
 int sm_shared_memory;
 int sm_registers;
 int sm_threads;
 int sm_blocks;
 int num_threads_per_warp;
} hip_device_property_t;


//*****************************************************************************
// interface operations
//*****************************************************************************

// returns 0 on success
int
hip_bind
(
 void
);

// returns 0 on success
int
hip_context
(
 hipCtx_t *ctx
);

// returns 0 on success
int
hip_device_property_query
(
 int device_id,
 hip_device_property_t *property
);

int
hip_dev_sync();

#endif //hip_api_h
