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
// Copyright ((c)) 2002-2023, Rice University
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
//   hip-api.c
//
// Purpose:
//   wrapper around AMD HIP layer
//
//***************************************************************************


//*****************************************************************************
// system include files
//*****************************************************************************

#include <dlfcn.h>
#include <stdio.h>
#include <string.h>    // memset

#include <hip/hip_runtime.h>

//*****************************************************************************
// local include files
//*****************************************************************************

#include <hpcrun/sample-sources/libdl.h>
#include <hpcrun/messages/messages.h>

#include "hip-api.h"



//*****************************************************************************
// macros
//*****************************************************************************

#define HIP_FN_NAME(f) DYN_FN_NAME(f)

#define HIP_FN(fn, args) \
  static hipError_t (*HIP_FN_NAME(fn)) args

#define HPCRUN_HIP_API_CALL(fn, args)                              \
{                                                                   \
  hipError_t error_result = HIP_FN_NAME(fn) args;                   \
  if (error_result != hipSuccess) {                                 \
    ETMSG(CUDA, "hip api %s returned %d", #fn, (int) error_result);    \
    exit(-1);                                                       \
  }                                                                 \
}

#define FORALL_HIP_ROUTINES(macro)             \
  macro(hipDeviceSynchronize)                  \
  macro(hipDeviceGetAttribute)                 \
  macro(hipCtxGetCurrent)

//******************************************************************************
// static data
//******************************************************************************

HIP_FN
(
 hipDeviceSynchronize,
( void )
);

HIP_FN
(
 hipDeviceGetAttribute,
 (
 int *pi,
 hipDeviceAttribute_t attrib,
 int dev
 )
);

HIP_FN
(
 hipCtxGetCurrent,
 (
 hipCtx_t *ctx
 )
);


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
hip_bind
(
void
)
{
  CHK_DLOPEN(hip, "libamdhip64.so", RTLD_NOW | RTLD_GLOBAL);

#define HIP_BIND(fn) \
  CHK_DLSYM(hip, fn);

  FORALL_HIP_ROUTINES(HIP_BIND);
#undef HIP_BIND

  return 0;
}

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
