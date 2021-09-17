// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2021, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

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
#include <cuda_runtime.h>
#include <stdbool.h>

#include "../gpu-op-placeholders.h"

#define HPCTOOLKIT_CUDA_SUCCESS 0
#define HPCTOOLKIT_CUDA_FAIL -1

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


// DRIVER_UPDATE_CHECK(Keren): reverse engineered
typedef struct {
  uint32_t unknown_field1[4];
  uint32_t function_index;
  uint32_t unknown_field2[3];
  CUmodule cumod;
} hpctoolkit_cufunc_st_t;


typedef struct {
  uint32_t cubin_id;
} hpctoolkit_cumod_st_t;


typedef struct {
  uint32_t unknown_field1[25];
  uint32_t context_id;
} hpctoolkit_cuctx_st_t;

//*****************************************************************************
// forward declaration
//*****************************************************************************

typedef struct cct_node_t cct_node_t;

// returns 0 on success

int 
cuda_bind
(
 void
);


int 
cuda_device_property_query
(
 int device_id, 
 cuda_device_property_t *property
);


int
cuda_global_pc_sampling_required
(
  int *required
);


int
cuda_context_sync
(
 CUcontext ctx
);


int
cuda_context_get
(
 CUcontext *ctx
);


int
cuda_context_set
(
 CUcontext ctx
);


bool
cuda_api_internal_get
(
);


void
cuda_api_enter_callback
(
 void *args,
 gpu_op_placeholder_flags_t flags
);


void
cuda_api_exit_callback
(
 void *args,
 gpu_op_placeholder_flags_t flags
);


cct_node_t *
cuda_api_node_get
(
);

//*****************************************************************************
// CUDA API operations
//*****************************************************************************
//
// returns 0 on success

cudaError_t
hpcrun_cudaLaunchKernel
(
 const void *func,
 dim3 gridDim,
 dim3 blockDim,
 void **args,
 size_t sharedMem,
 cudaStream_t stream
);

cudaError_t
hpcrun_cudaMemcpy
(
 void *dst,
 const void *src,
 size_t count,
 enum cudaMemcpyKind kind
);

CUresult
hpcrun_cuLaunchKernel
(
 CUfunction f,
 unsigned int gridDimX,
 unsigned int gridDimY,
 unsigned int gridDimZ,
 unsigned int blockDimX,
 unsigned int blockDimY,
 unsigned int blockDimZ,
 unsigned int sharedMemBytes,
 CUstream hStream,
 void **kernelParams,
 void **extra
);

CUresult
hpcrun_cuMemcpy
(
 CUdeviceptr dst,
 CUdeviceptr src,
 size_t ByteCount
);

CUresult
hpcrun_cuMemcpyHtoD_v2
(
 CUdeviceptr dstDevice,
 const void *srcHost,
 size_t ByteCount
);

CUresult
hpcrun_cuMemcpyDtoH_v2
(
 void *dstHost,
 CUdeviceptr srcDevice,
 size_t ByteCount
);

#endif
