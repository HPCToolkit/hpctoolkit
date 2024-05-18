// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//***************************************************************************
//
// File:
//   cuda-api.h
//
// Purpose:
//   interface definitions for wrapper around NVIDIA CUDA layer
//
//***************************************************************************

#ifndef cuda_api_h
#define cuda_api_h



//*****************************************************************************
// nvidia includes
//*****************************************************************************

#include <cuda.h>



//*****************************************************************************
// interface operations
//*****************************************************************************

typedef struct cuda_device_property {
  int sm_count;
  int sm_clock_rate;
  int sm_shared_memory;
  int sm_registers;
  int sm_threads;
  int sm_blocks;
  int num_threads_per_warp;
} cuda_device_property_t;


//*****************************************************************************
// interface operations
//*****************************************************************************

// returns 0 on success
int
cuda_bind
(
 void
);


// returns 0 on success
int
cuda_context
(
 CUcontext *ctx
);


// returns 0 on success
int
cuda_device_property_query
(
 int device_id,
 cuda_device_property_t *property
);


// returns 0 on success
int
cuda_global_pc_sampling_required
(
  int *required
);


// returns 0 on success
int
cuda_get_module
(
 CUmodule *mod,
 CUfunction fn
);


// returns 0 on success
int
cuda_get_driver_version
(
 int *major,
 int *minor
);


// returns 0 on success
int
cuda_get_runtime_version
(
 int *major,
 int *minor
);


// returns 0 on success
int
cuda_get_code
(
 void* host,
 const void* dev,
 size_t bytes
);

#endif
