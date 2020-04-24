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
// Copyright ((c)) 2002-2020, Rice University
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
//   cuda-api.c
//
// Purpose:
//   wrapper around NVIDIA CUDA layer
//
//***************************************************************************


//*****************************************************************************
// system include files
//*****************************************************************************

#include <dlfcn.h>
#include <stdio.h>
#include <string.h>    // memset

#include <cuda.h>



//*****************************************************************************
// local include files
//*****************************************************************************

#include <hpcrun/sample-sources/libdl.h>
#include <hpcrun/messages/messages.h>

#include "cuda-api.h"



//*****************************************************************************
// macros
//*****************************************************************************

#define CUDA_FN_NAME(f) DYN_FN_NAME(f)

#define CUDA_FN(fn, args) \
  static CUresult (*CUDA_FN_NAME(fn)) args

#define HPCRUN_CUDA_API_CALL(fn, args)                              \
{                                                                   \
  CUresult error_result = CUDA_FN_NAME(fn) args;		    \
  if (error_result != CUDA_SUCCESS) {				    \
    ETMSG(CUDA, "cuda api %s returned %d", #fn, (int) error_result);    \
    exit(-1);							    \
  }								    \
}



//******************************************************************************
// static data
//******************************************************************************

#ifndef HPCRUN_STATIC_LINK
CUDA_FN
(
 cuDeviceGetAttribute,
 (
  int* pi,
  CUdevice_attribute attrib,
  CUdevice dev
 )
);


CUDA_FN
(
 cuCtxGetCurrent,
 (
  CUcontext *ctx
 )
);
#endif


//******************************************************************************
// private operations
//******************************************************************************

int
cuda_bind
(
  void
)
{
#ifndef HPCRUN_STATIC_LINK
  // dynamic libraries only availabile in non-static case
  CHK_DLOPEN(cuda, "libcuda.so", RTLD_NOW | RTLD_GLOBAL);

  CHK_DLSYM(cuda, cuDeviceGetAttribute);
  CHK_DLSYM(cuda, cuCtxGetCurrent);

  return 0;
#else
  return -1;
#endif // ! HPCRUN_STATIC_LINK
}


#ifndef HPCRUN_STATIC_LINK
static int
cuda_device_sm_blocks_query
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
#endif


//******************************************************************************
// interface operations
//******************************************************************************

int
cuda_context
(
 CUcontext *ctx
)
{
#ifndef HPCRUN_STATIC_LINK
  HPCRUN_CUDA_API_CALL(cuCtxGetCurrent, (ctx));
  return 0;
#else
  return -1;
#endif
}

int
cuda_device_property_query
(
 int device_id,
 cuda_device_property_t *property
)
{
#ifndef HPCRUN_STATIC_LINK
  HPCRUN_CUDA_API_CALL(cuDeviceGetAttribute,
    (&property->sm_count, CU_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT, device_id));

  HPCRUN_CUDA_API_CALL(cuDeviceGetAttribute,
    (&property->sm_clock_rate, CU_DEVICE_ATTRIBUTE_CLOCK_RATE, device_id));

  HPCRUN_CUDA_API_CALL(cuDeviceGetAttribute,
    (&property->sm_shared_memory,
     CU_DEVICE_ATTRIBUTE_MAX_SHARED_MEMORY_PER_MULTIPROCESSOR, device_id));

  HPCRUN_CUDA_API_CALL(cuDeviceGetAttribute,
    (&property->sm_registers,
     CU_DEVICE_ATTRIBUTE_MAX_REGISTERS_PER_MULTIPROCESSOR, device_id));

  HPCRUN_CUDA_API_CALL(cuDeviceGetAttribute,
    (&property->sm_threads, CU_DEVICE_ATTRIBUTE_MAX_THREADS_PER_MULTIPROCESSOR,
     device_id));

  HPCRUN_CUDA_API_CALL(cuDeviceGetAttribute,
    (&property->num_threads_per_warp, CU_DEVICE_ATTRIBUTE_WARP_SIZE,
     device_id));

  int major = 0, minor = 0;

  HPCRUN_CUDA_API_CALL(cuDeviceGetAttribute,
    (&major, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, device_id));

  HPCRUN_CUDA_API_CALL(cuDeviceGetAttribute,
    (&minor, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR, device_id));

  property->sm_blocks = cuda_device_sm_blocks_query(major, minor);

  return 0;
#else
  return -1;
#endif
}
