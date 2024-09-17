// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//***************************************************************************
//
// File:
//   hip-api.c
//
// Purpose:
//   wrapper around AMD HIP layer
//
//***************************************************************************


//*****************************************************************************
// system include files
//*****************************************************************************

#define _GNU_SOURCE

#include <dlfcn.h>
#include <stdio.h>
#include <string.h>    // memset

//*****************************************************************************
// local include files
//*****************************************************************************

#include "../../../foil/rocm-hip.h"
#include "../../../messages/messages.h"

#include "hip-api.h"



//*****************************************************************************
// macros
//*****************************************************************************

#define HPCRUN_HIP_API_CALL(fn, args)                              \
{                                                                   \
  hipError_t error_result = f_##fn args;                   \
  if (error_result != hipSuccess) {                                 \
    ETMSG(CUDA, "hip api %s returned %d", #fn, (int) error_result);    \
    exit(-1);                                                       \
  }                                                                 \
}

//******************************************************************************
// private operations
//******************************************************************************
//TODO: Copied from cuda-api.c - check if works for hip
static int
hip_device_sm_blocks_query
(
 int major,
 int minor
)
{
  switch(major) {
    case 7:
    case 6:
      return 32;
    default:
      // TODO(Keren): add more devices
      return 8;
  }
}


//******************************************************************************
// interface operations
//******************************************************************************

int
hip_context
(
 hipCtx_t *ctx
)
{
  HPCRUN_HIP_API_CALL(hipCtxGetCurrent, (ctx));
  return 0;
}

int
hip_device_property_query
(
 int device_id,
 hip_device_property_t *property
)
{
  HPCRUN_HIP_API_CALL(hipDeviceGetAttribute,
                       (&property->sm_count, hipDeviceAttributeMultiprocessorCount, device_id));

  HPCRUN_HIP_API_CALL(hipDeviceGetAttribute,
                       (&property->sm_clock_rate, hipDeviceAttributeClockRate, device_id));

  HPCRUN_HIP_API_CALL(hipDeviceGetAttribute,
                       (&property->sm_shared_memory,
                       hipDeviceAttributeMaxSharedMemoryPerMultiprocessor, device_id));

  HPCRUN_HIP_API_CALL(hipDeviceGetAttribute,
                       (&property->sm_registers,
                       hipDeviceAttributeMaxRegistersPerBlock, device_id));//CU_DEVICE_ATTRIBUTE_MAX_REGISTERS_PER_MULTIPROCESSOR

  HPCRUN_HIP_API_CALL(hipDeviceGetAttribute,
                       (&property->sm_threads, hipDeviceAttributeMaxThreadsPerMultiProcessor,
                       device_id));

  HPCRUN_HIP_API_CALL(hipDeviceGetAttribute,
                       (&property->num_threads_per_warp, hipDeviceAttributeWarpSize,
                       device_id));

  int major = 0, minor = 0;

  HPCRUN_HIP_API_CALL(hipDeviceGetAttribute,
                       (&major, hipDeviceAttributeComputeCapabilityMajor, device_id));

  HPCRUN_HIP_API_CALL(hipDeviceGetAttribute,
                       (&minor, hipDeviceAttributeComputeCapabilityMinor, device_id));

  property->sm_blocks = hip_device_sm_blocks_query(major, minor);

  return 0;
}


int
hip_dev_sync
()
{
  HPCRUN_HIP_API_CALL(hipDeviceSynchronize, () );
  return 0;
}
